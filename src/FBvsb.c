/******************************************************************************
 * 
 * TANAKA ENTERPRISES CONFIDENTIAL
 * __________________
 * 
 *  [2015] - [2016] Tanaka Enterprises
 *  All Rights Reserved.
 * 
 * NOTICE:  All information contained herein is, and remains
 * the property of Tanaka Enterprises and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Tanaka Enterprises
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Tanaka Enterprises.
 * 
 *****************************************************************************/

/**
 * @file FBvsb.c
 * @brief This file implements a variable speed buffer module.
 * @version $LastChangedRevision:$
 * @author Ryo Tanaka
 */
#include "yavc.h"
#include "vectors.h"
#include "vectors_cplx.h"
#include "CircularBuf.h"
#include "FB.h"
#include "FB_filters.h"
#include "yavc_fft.h"
#include "FBvsb.h"

/* modify buffer index to be within the range */
#define ADJIDX(idx, start, end, size)  do {		\
    if (start < end) {                                  \
      while(idx < start) idx += (end-start);            \
      while(idx >= end)  idx -= (end-start);            \
    }                                                   \
    else {                                              \
      if (idx < (start + end)/2) idx += size;           \
      while(idx < start)     idx += (size-start+end);   \
      while(idx >= end+size) idx -= (size-start+end);   \
      if (idx >= size) idx -= size;                     \
    }                                                   \
  } while(0)

/*****************
 *    Tool       *
 *****************/
static void vec_angle(float * restrict angle, const rv_complex * X, int nband)
{
  int i;
  for (i=0; i<nband; i++) {
    angle[i] = (float)ATAN2(X[i].i, X[i].r);
  }
}

/************************************
 *    Implementation of FBVSB       *
 ************************************/
typedef struct {
  rv_complex * restrict X;
  rv_complex * restrict X1;
  float      * restrict lev;
  float      * restrict ph;
} HISTORY;

typedef struct {
  HISTORY          * hist;
  SimpleCBState    * tdbuf;
  float            * levn;
  float            * phn;
  FBsynthesisState * syn;  
} CH;

struct FBVSB_State_ {
  AC4_HEADER;
  /* configuration and history */
  int    fs;
  int    frame_size;
  int    fft_size;
  int    fbfilter_size;
  int    nch;
  int    nband;
  int    nhist;
  CH     * ch;
  float  * ha;
  
  float  fpos;
  int    loop_len;
  int    loop_start;
  int    loop_end;

  /* Audio Components */
  yavc_fft_state * restrict fft;
  
  /* parameters */
  float loopgain;
  float feedbackgain;
  float recgain;
} ;

static const RegDef_t rd[] = {
  AC_REGDEF(loopgain,     CLI_ACFPTM,   FBVSB_State, "Loopback gain"),
  AC_REGDEF(feedbackgain, CLI_ACFPTM,   FBVSB_State, "Feedback gain"),
  AC_REGDEF(recgain,      CLI_ACFPTM,   FBVSB_State, "Recording gain"),
};

FBVSB_State *FBvsb_init(const char * name, int nch, int fs, int frame_size, float length_sec)
{
  int ch, i;
  FBVSB_State* st;
  const FB_filters_info * fbinfo;

  int fft_size      = 2 * frame_size;
  int fbfilter_size = 3 * fft_size;
  int nband         = fft_size / 2;

  fbinfo = FB_filters_getinfo(frame_size, fft_size, fbfilter_size);
  
  st = MEM_ALLOC(MEM_SDRAM, FBVSB_State, 1, 8);

  AC4_ADD(name, st, "FBVSB");
  AC4_ADD_REG(rd);

  st->fs            = fs;
  st->frame_size    = frame_size;
  st->fft_size      = fft_size;
  st->fbfilter_size = fbfilter_size;
  st->nch           = nch;
  st->nband         = nband;
  st->nhist         = (int)(length_sec * fs / frame_size);
  st->loop_len      = st->nhist * st->frame_size;
  st->loop_start    = 0;
  st->loop_end      = st->loop_len;

  st->ha = MEM_ALLOC(MEM_SDRAM, float, fbfilter_size, 8);
  COPY(st->ha, fbinfo->filter_h, fbfilter_size);

  st->ch = MEM_ALLOC(MEM_SDRAM, CH, nch, 4);
  for (ch = 0; ch < nch; ch++) {
    st->ch[ch].hist = MEM_ALLOC(MEM_SDRAM, HISTORY, st->nhist, 8);
    for (i = 0; i < st->nhist; i++) {
      st->ch[ch].hist[i].X   = MEM_ALLOC(MEM_SDRAM, rv_complex, nband+1, 8);
      st->ch[ch].hist[i].X1  = MEM_ALLOC(MEM_SDRAM, rv_complex, nband+1, 8);
      st->ch[ch].hist[i].lev = MEM_ALLOC(MEM_SDRAM, float,      nband,   8);
      st->ch[ch].hist[i].ph  = MEM_ALLOC(MEM_SDRAM, float,      nband,   8);
    }
    st->ch[ch].tdbuf = simplecb_init(fbfilter_size + frame_size);
    st->ch[ch].levn  = MEM_ALLOC(MEM_SDRAM, float, nband,   8);
    st->ch[ch].phn   = MEM_ALLOC(MEM_SDRAM, float, nband,   8);
    st->ch[ch].syn   = FBsynthesis_init(fbinfo->fftsize, fbinfo->decimation, fbinfo->lg, fbinfo->filter_g, 1);
  }

  st->fft           = yavc_fft_r2c_init(fft_size);
  
  st->fpos          = 0.0f;
  st->loopgain      = 1.0f;
  st->feedbackgain  = 1.0f;
  st->recgain       = 1.0f;
	
  return st;
}

static void proc_put(FBVSB_State * restrict st, float* src[], float* speed)
{
  int i,m,ch;
  int frame_size    = st->frame_size;
  int fbfilter_size = st->fbfilter_size;
  int fft_size      = st->fft_size;
  int nch           = st->nch;
  int nband         = st->nband;
  int nhist         = st->nhist;
  int cur           = (int)(st->fpos / frame_size);
  int putlen        = 0;
  CH * chst;

  VARDECLR(float,      fftbuf);
  VARDECLR(rv_complex, fdtemp);
  VARDECLR(float,      sctemp);
  SAVE_STACK;
  ALLOC(fftbuf, fbfilter_size, float);
  ALLOC(fdtemp, nband+1,       rv_complex);
  ALLOC(sctemp, nband+1,       float);
  
  for (i=0; i<frame_size; i++) {
    st->fpos += speed[i];
    ADJIDX(st->fpos, st->loop_start, st->loop_end, nhist * frame_size);
    putlen++;
    if ((int)(st->fpos / frame_size) != cur) {
      for (ch=0; ch<nch; ch++) {
      	chst = &st->ch[ch];
	      simplecb_write(chst->tdbuf, &src[ch][i-putlen+1], putlen);
	
        // FB analysis
        simplecb_read(chst->tdbuf, fftbuf, fbfilter_size, 0);
        if (st->recgain > 0.f) {
          vec_mul1(fftbuf, st->ha, fbfilter_size);
          for (m=0; m<fbfilter_size/fft_size-1; m++) {
            vec_add1(fftbuf, &fftbuf[(m+1)*fft_size], fft_size);
          }
          yavc_fft_r2c(st->fft, fdtemp, fftbuf);
          vec_cplx_wadd1(chst->hist[cur].X, st->feedbackgain, st->recgain, fdtemp, nband);
        }

        simplecb_read(chst->tdbuf, fftbuf, fbfilter_size, frame_size);
        if (st->recgain > 0.f) {
          vec_mul1(fftbuf, st->ha, fbfilter_size);
          for (m=0; m<fbfilter_size/fft_size-1; m++) {
            vec_add1(fftbuf, &fftbuf[(m+1)*fft_size], fft_size);
          }
          yavc_fft_r2c(st->fft, fdtemp, fftbuf);
          vec_cplx_wadd1(chst->hist[cur].X1, st->feedbackgain, st->recgain, fdtemp, nband);
        }

        // level
        vec_spectral_power(sctemp, chst->hist[cur].X, nband);
        vec_sqrt(chst->hist[cur].lev, sctemp, nband);
	
        // phase analysis
        vec_cplx_mul_conj(fdtemp, chst->hist[cur].X1, chst->hist[cur].X, nband);
        vec_angle(chst->hist[cur].ph, fdtemp, nband);
        // !!todo!! adjust fractional delay
      }
      putlen = 0;
      cur = (int)(st->fpos / frame_size);
    }
  }
  for (ch=0; ch<nch; ch++) {
    simplecb_write(st->ch[ch].tdbuf, &src[ch][frame_size-putlen], putlen);
  }

  RESTORE_STACK;
}

static void proc_get(FBVSB_State * restrict st, float* dst[], float* speed)
{
  int i, ch;
  float pi_2        = 2.f * (float)M_PI;
  float rc_pi_2     = RECIP(pi_2);
  int frame_size    = st->frame_size;
  int nch           = st->nch;
  int nband         = st->nband;
  int nhist         = st->nhist;
  float fpos        = st->fpos;
  int   cur, cur1;
  float bal, bal1;
  CH * chst;

  VARDECLR(float,      lev);
  VARDECLR(float,      ph);
  VARDECLR(rv_complex, X);
  SAVE_STACK;
  ALLOC(lev, nband,   float);
  ALLOC(ph,  nband,   float);
  ALLOC(X,   nband+1, rv_complex);
  
  fpos += vec_sum(speed, st->frame_size);
  ADJIDX(fpos, st->loop_start, st->loop_end, nhist * frame_size);
  cur = (int)(fpos / frame_size);
  cur1 = cur < nhist-1 ? cur + 1 : 0;
  bal1 = (fpos - cur * frame_size) * RECIP((float)frame_size);
  bal  = 1.f - bal1;

  for (ch=0; ch<nch; ch++) {
    chst = &st->ch[ch];
    
    vec_wadd(lev, bal, chst->hist[cur].lev, bal1, chst->hist[cur1].lev, nband);
    vec_wadd(ph,  bal, chst->hist[cur].ph,  bal1, chst->hist[cur1].ph,  nband);

    COPY(chst->levn, lev, nband);
    for (i=0; i<nband; i++) {
      chst->phn[i] += ph[i];
      chst->phn[i] -= pi_2 * (int)(chst->phn[i] * rc_pi_2 + .5f);
    }
  
    /* generate new level spectrum */
    for (i=0; i<nband; i++) {
      X[i].r = chst->levn[i] * (float)COS(chst->phn[i]);
      X[i].i = chst->levn[i] * (float)SIN(chst->phn[i]);
    }
    CPX_ZERO(X[nband]);
    FBsynthesis_process(chst->syn, dst[ch], X);
  }

  RESTORE_STACK;
}


void FBvsb_process(FBVSB_State * restrict st, float* dst[], float* src[], float* speed)
{
  int ch;
  int nch        = st->nch;
  int frame_size = st->frame_size;
  
  proc_get(st, dst, speed);
  for (ch=0; ch<nch; ch++) {
    vec_mul1s(dst[ch], st->loopgain * st->feedbackgain, frame_size);
  }

  proc_put(st, src, speed);
}

int FBvsb_get_buflen(FBVSB_State * restrict st)
{
  return st->nhist * st->frame_size;
}

void FBvsb_set_loop(FBVSB_State * restrict st, int loop_start, int loop_len)
{
  int size = st->nhist * st->frame_size;
  if (loop_start < size && st->loop_len <= size) {
    float offset = st->fpos - st->loop_start;
    while (offset < 0.f) offset += (size);
    st->loop_start = loop_start;
    st->loop_len   = loop_len;
    st->loop_end = st->loop_start + st->loop_len;
    while(st->loop_end > size) st->loop_end -= size;
    if (st->loop_start < st->loop_end) {
      if (st->fpos < st->loop_start || st->fpos >= st->loop_end) {
        st->fpos = st->loop_start + offset;
      }
    }
    else {
      if (st->fpos >= st->loop_end && st->fpos < st->loop_start) {
        st->fpos = st->loop_start + offset;
      }
    }
    ADJIDX(st->fpos, st->loop_start, st->loop_end, size);
  }
}

void FBvsb_set_pos(FBVSB_State * restrict st, float fpos, float fpos_prev, float speed_prev)
{
  ADJIDX(fpos, st->loop_start, st->loop_end, st->nhist * st->frame_size);
  st->fpos = fpos;
  (void)fpos_prev;
  (void)speed_prev;  
}

void FBvsb_set_feedbackgain(FBVSB_State * restrict st, float feedbackgain)
{
  st->feedbackgain = feedbackgain;
}

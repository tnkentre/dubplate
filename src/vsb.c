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
 * @file vsb.c
 * @brief This file implements a variable speed buffer module.
 * @version $LastChangedRevision:$
 * @author Ryo Tanaka
 */
#include "yavc.h"
#include "vectors.h"
#include "vsb.h"

/* modify buffer index to be within the range */
#define ADJIDX(idx, start, end, size)  do {		\
    if (start < end) {                                  \
      while(idx < start) idx += (end-start);            \
      while(idx >= end)  idx -= (end-start);            \
    }                                                   \
    else {                                              \
      if (idx < (start + end)/2) idx += size;           \
      while(idx < start)     idx += (size-start+end);	\
      while(idx >= end+size) idx -= (size-start+end);   \
      if (idx >= size) idx -= size;                     \
    }                                                   \
  } while(0)


struct VSB_State_ {
  AC4_HEADER;
  /* configuration and history */
  int    size;
  float* buf[2];
  float  fpos;
  float  fpos_prev;
  float  speed_prev;
  float  src_prev[2];
  int    loop_len;
  int    loop_start;
  int    loop_end;

  /* parameters */
  float loopgain;
  float feedbackgain;
  float recgain;
} ;

static const RegDef_t rd[] = {
  AC_REGDEF(loopgain,     CLI_ACFPTM,   VSB_State, "Loopback gain"),
  AC_REGDEF(feedbackgain, CLI_ACFPTM,   VSB_State, "Feedback gain"),
  AC_REGDEF(recgain,      CLI_ACFPTM,   VSB_State, "Recording gain"),
  AC_REGDEF(loop_start,   CLI_ACIPTM,   VSB_State, "start index of loop"),
  AC_REGDEF(loop_end,     CLI_ACIPTM,   VSB_State, "end index of loop"),
  AC_REGDEF(loop_len,     CLI_ACIPTM,   VSB_State, "lentgh of loop"),
  AC_REGDEF(fpos,         CLI_ACFPTM,   VSB_State, "buffer index"),
  AC_REGDEF(size,         CLI_ACIPTM,   VSB_State, "buffer size"),
};

VSB_State *vsb_init(const char * name, int size)
{
  VSB_State* st;
  st = MEM_ALLOC(MEM_SDRAM, VSB_State, 1, 8);

  AC4_ADD(name, st, "VSB");
  AC4_ADD_REG(rd);

  st->size = size;
  st->buf[0] = MEM_ALLOC(MEM_SDRAM, float, st->size, 8);
  st->buf[1] = MEM_ALLOC(MEM_SDRAM, float, st->size, 8);
  vec_set(st->buf[0], 0.0f, st->size);
  vec_set(st->buf[1], 0.0f, st->size);
  st->loopgain     = 1.0f;
  st->feedbackgain = 1.0f;
  st->recgain      = 1.0f;
  st->loop_len = st->size;
  st->loop_start = 0;
  st->loop_end = st->loop_len;
	
  return st;
}

void vsb_process(VSB_State * restrict st, float* dst[], float* src[], float* speed, int len)
{
  int   i, j;
  int   ipos;
  int   ipos_prev;
  int   ipos_temp;
  float fpos;
  float fpos_prev;
  float src_prev[2];
  float speed_prev;
  float bal0, bal1;
  int   bufidx;

  for (i=0; i<len; i++) {

    // Get history
    fpos_prev   = st->fpos_prev;
    fpos        = st->fpos;
    src_prev[0] = st->src_prev[0];
    src_prev[1] = st->src_prev[1];
    speed_prev  = st->speed_prev;

    // prepare integer index
    ipos_prev   = (int)fpos_prev;
    ipos        = (int)fpos;

    // get recorded data
    bal1   = fpos - (float)ipos;
    bal0   = 1.0 - bal1;
    bufidx = ipos+1;
    ADJIDX(bufidx, st->loop_start, st->loop_end, st->size);
    if (speed_prev==0.0f) {
      dst[0][i] = 0.f;
      dst[1][i] = 0.f;
    }
    else {
      dst[0][i] = st->loopgain * st->feedbackgain * (bal0 * st->buf[0][ipos] + bal1 * st->buf[0][bufidx]);
      dst[1][i] = st->loopgain * st->feedbackgain * (bal0 * st->buf[1][ipos] + bal1 * st->buf[1][bufidx]);
    }

    // --- write data ---
    if (ipos != ipos_prev) {
      if (speed_prev > 0.f) {
        // writing during ipos_prev --> ipos
        if (st->loop_start < st->loop_end) {
          ipos_temp = ipos > ipos_prev ? ipos : ipos + st->loop_len;
        }
        else {
          if (ipos < st->loop_end && ipos_prev >= st->loop_start)
            ipos_temp = ipos + st->size;
          else if (ipos_prev < st->loop_end && ipos >= st->loop_start) {
            ipos_temp = ipos - (st->loop_start - st->loop_end);
          }
          else {
            ipos_temp = ipos;
          }
        }
        for (j=ipos_prev+1; j<=ipos_temp; j++) {
          bal1 = ((float)j - fpos_prev) / (fpos - fpos_prev);
          bal0 = 1.f - bal1;
          bufidx = j;
          ADJIDX(bufidx, st->loop_start, st->loop_end, st->size);
          st->buf[0][bufidx] *= st->feedbackgain;
          st->buf[1][bufidx] *= st->feedbackgain;
          st->buf[0][bufidx] += st->recgain * (bal0 * src_prev[0] + bal1 * src[0][i]);
          st->buf[1][bufidx] += st->recgain * (bal0 * src_prev[1] + bal1 * src[1][i]);
        }
      }
      else if (speed_prev < 0.f) {
        // writing during ipos_prev --> ipos        
        if (st->loop_start < st->loop_end) {
          ipos_temp = ipos < ipos_prev ? ipos : ipos - st->loop_len;
        }
        else {
          if (ipos_prev < st->loop_end && ipos >= st->loop_start)
            ipos_temp = ipos - st->size;
          else if (ipos < st->loop_end && ipos_prev >= st->loop_start) {
            ipos_temp = ipos + (st->loop_start - st->loop_end);
          }
          else {
            ipos_temp = ipos;
          }
        }
        for (j=ipos_prev; j>ipos_temp; j--) {
          bal1 = (fpos_prev - (float)j) / (fpos_prev - fpos);
          bal0 = 1.f - bal1;
          bufidx = j;
          ADJIDX(bufidx, st->loop_start, st->loop_end, st->size);
          st->buf[0][bufidx] *= st->feedbackgain;
          st->buf[1][bufidx] *= st->feedbackgain;
          st->buf[0][bufidx] += st->recgain * (bal0 * src_prev[0] + bal1 * src[0][i]);
          st->buf[1][bufidx] += st->recgain * (bal0 * src_prev[1] + bal1 * src[1][i]);
        }
      }
    }

    // update history
    fpos += speed[i];
    ADJIDX(fpos, st->loop_start, st->loop_end, st->size);
    st->fpos_prev   = st->fpos;
    st->fpos        = fpos;
    st->src_prev[0] = src[0][i];
    st->src_prev[1] = src[1][i];
    st->speed_prev  = speed[i];
  }
}

void vsb_set_feedbackgain(VSB_State * restrict st, float feedbackgain)
{
  st->feedbackgain = feedbackgain;
}

void vsb_set_loop(VSB_State * restrict st, int loop_start, int loop_len)
{
  if (loop_start < st->size && st->loop_len <= st->size) {
    float offset = st->fpos - st->loop_start;
    while (offset < 0.f) offset += (st->size);
    st->loop_start = loop_start;
    st->loop_len   = loop_len;
    st->loop_end = st->loop_start + st->loop_len;
    while(st->loop_end > st->size) st->loop_end -= st->size;
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
    ADJIDX(st->fpos, st->loop_start, st->loop_end, st->size);
  }
}

float vsb_get_pos(VSB_State * restrict st)
{
  return st->fpos;
}

float vsb_vinylpos(VSB_State * restrict st)
{
  return st->fpos / (float)(st->size);
}


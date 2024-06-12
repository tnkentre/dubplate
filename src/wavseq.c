/*******************************************************************************
 *
 * REVOLABS CONFIDENTIAL
 * __________________
 *
 *  [2005] - [2016] Revolabs Incorporated
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Revolabs Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Revolabs Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Revolabs Incorporated.
 *
 ******************************************************************************/

/**
 * @file wavseq.c
 * @brief wavseq processing
 * @version $LastChangedRevision:$
 * @author Ryo Tanaka
 */
#include "yavc.h"
#include "vectors.h"
#include "tcanalysis.h"
#include "vsb.h"
#include "FBvsb.h"
#include "wav.h"
#include "wavseq.h"

#define BANK_NUM (5)
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

/*
 * wavseq functions
 */
/* wavseq State */
struct wavseq_state_ {
  AC4_HEADER;

  /* Configuration */
  int fs;
  int frame_size;
  int bar_max;
  float bpm_base;
  int xy_len; // for OSC

  /* Parameters */
  int banksel;
  float recgain;
  float recfade;
  float outgain;
  float fdbal;
  float beat0;
  float beat1;
  float beat2;
  float beat3;
  float beat4;
  float beat5;
  float beat6;
  float beat7;
  float* envelope;
  float envdcy_min;
  float trackpos;

  /* Internal */
  int buflen;
  double lastpos;
  int banksel_;
  float recgain_;
  int   recend;
  
  float rec_startspeed;
  float rec_len;

  /* Audio Components */
  WavState         * wav;
  VSB_State        * tdvsb[BANK_NUM];
  FBVSB_State      * fdvsb[BANK_NUM];
} ;

static const RegDef_t rd[] = {
  AC_REGDEF(banksel,    CLI_ACIPTM, wavseq_state, "Bank selection"),
  AC_REGDEF(recgain,    CLI_ACFPTM, wavseq_state, "Recording gain"),
  AC_REGDEF(recfade,    CLI_ACFPTM, wavseq_state, "Fade out of recording"),
  AC_REGDEF(outgain,    CLI_ACFPTM, wavseq_state, "Output gain"),
  AC_REGDEF(beat0,      CLI_ACFPTM, wavseq_state, "beat0"),
  AC_REGDEF(beat1,      CLI_ACFPTM, wavseq_state, "beat1"),
  AC_REGDEF(beat2,      CLI_ACFPTM, wavseq_state, "beat2"),
  AC_REGDEF(beat3,      CLI_ACFPTM, wavseq_state, "beat3"),
  AC_REGDEF(beat4,      CLI_ACFPTM, wavseq_state, "beat4"),
  AC_REGDEF(beat5,      CLI_ACFPTM, wavseq_state, "beat5"),
  AC_REGDEF(beat6,      CLI_ACFPTM, wavseq_state, "beat6"),
  AC_REGDEF(beat7,      CLI_ACFPTM, wavseq_state, "beat7"),
  AC_REGDEF(fdbal,      CLI_ACFPTM, wavseq_state, "Balance to FD"),
  AC_REGDEF(rec_len,    CLI_ACFPTM, wavseq_state, "Recording length"),
  AC_REGDEF(trackpos,   CLI_ACFPTM, wavseq_state, "track position [range : 0 - 1]"),
  AC_REGDEF(envdcy_min, CLI_ACFPTM, wavseq_state, "Minimum value of envelope decay [range : 0 - 1]"),
  AC_REGADEF(envelope,  xy_len, CLI_ACFPTMA,   wavseq_state, "position"),
};

wavseq_state* wavseq_init(const char *name, int fs, int frame_size, int bar_max)
{
  int ch, fr;
  wavseq_state* st;
  char subname[32];
  
  st = MEM_ALLOC(MEM_SDRAM, wavseq_state, 1, 4);
  AC4_ADD(name, st, "wavseq");
  AC4_ADD_REG(rd);

  st->fs          = fs;
  st->frame_size  = frame_size;
  st->bar_max     = bar_max;
  st->bpm_base    = 120.f;
  st->xy_len      = 2;

  st->banksel     = 0;
  st->recgain     = 0.0f;
  st->recfade     = dBtoM(-6.0 / (0.02f * fs) * frame_size);
  st->outgain     = 1.0;
  st->beat0       = 1.f;
  st->beat1       = 1.f;
  st->beat2       = 1.f;
  st->beat3       = 1.f;
  st->beat4       = 1.f;
  st->beat5       = 1.f;
  st->beat6       = 1.f;
  st->beat7       = 1.f;
  st->fdbal       = 0.0f;
  st->envelope    = MEM_ALLOC(MEM_SDRAM, float, st->xy_len, 4);
  st->envelope[0] = 1.f;
  st->envelope[1] = 1.f;
  st->envdcy_min  = 0.02f;

  ch = 0;
  sprintf(subname, "%s_fdvsb%d",name, ch);
  float sec = 60.f / st->bpm_base * 4 * st->bar_max;
  st->fdvsb[ch] = FBvsb_init(subname, 2, st->fs, st->frame_size, sec);

  st->buflen = FBvsb_get_buflen(st->fdvsb[ch]);

  sprintf(subname, "%s_tdvsb%d",name, ch);
  st->tdvsb[ch] = vsb_init(subname, st->buflen);

  /* TODO VSB for wav files */
  sprintf(subname, "%s_wav",name);
  st->wav = wav_init(subname, st->fs);

  VARDECLR(float*, input);
  VARDECLR(float*, output);
  VARDECLR(float,  speed);
  SAVE_STACK;
  ALLOC(input, 2, float*);
  ALLOC(output, 2, float*);
  ALLOC(input[0], st->frame_size, float);
  ALLOC(input[1], st->frame_size, float);
  ALLOC(output[0], st->frame_size, float);
  ALLOC(output[1], st->frame_size, float);
  ALLOC(speed, st->frame_size, float);

  for (ch=1; ch<BANK_NUM; ch++) {
    char filename[256];
    sprintf(filename, "/Users/ryo.tanaka/Music/dubplate_samples/%s/s%d.wav", name, ch-1);
    if(wav_open_FileRead(st->wav, filename) < 0) continue;

    TRACE(LEVEL_INFO, "Audio file loaded : %s", filename);
    sec = wav_get_length_sec(st->wav);
    sprintf(subname, "%s_fdvsb%d",name, ch);
    st->fdvsb[ch] = FBvsb_init(subname, 2, st->fs, st->frame_size, sec);
    sprintf(subname, "%s_tdvsb%d",name, ch);
    st->tdvsb[ch] = vsb_init(subname, FBvsb_get_buflen(st->fdvsb[ch]));

    vec_set(speed, 1.f, st->frame_size);
    int nframes = FBvsb_get_buflen(st->fdvsb[ch]) / st->frame_size;
    for (fr=0; fr<nframes; fr++) {
      wav_read(st->wav, input, st->frame_size);
      FBvsb_process(st->fdvsb[ch], output, input, speed, NULL);
      vsb_process(st->tdvsb[ch], output, input, speed, NULL, st->frame_size);
    }
    FBvsb_set_pos(st->fdvsb[ch], 0.f, 0.f, 0.f);
    vsb_set_pos(st->tdvsb[ch], 0.f, 0.f, 0.f);
  }

  RESTORE_STACK;

  /* OSC Monitoring */
  yavc_osc_add_acreg((char*)name, "banksel");
  yavc_osc_add_acreg((char*)name, "recgain");
  yavc_osc_add_acreg((char*)name, "outgain");
  yavc_osc_add_acreg((char*)name, "trackpos");
  yavc_osc_add_acreg((char*)name, "beat0");
  yavc_osc_add_acreg((char*)name, "beat1");
  yavc_osc_add_acreg((char*)name, "beat2");
  yavc_osc_add_acreg((char*)name, "beat3");
  yavc_osc_add_acreg((char*)name, "beat4");
  yavc_osc_add_acreg((char*)name, "beat5");
  yavc_osc_add_acreg((char*)name, "beat6");
  yavc_osc_add_acreg((char*)name, "beat7");
  yavc_osc_add_acreg((char*)name, "envelope");

  return st;
}

static void compute_envelope(wavseq_state* st, float* envelope, const double* position)
{
  int i;
  int frame_size = st->frame_size;

  for (i=0; i<frame_size; i++) {
    float fbeat = (float)(position[i] - (int)position[i]) * 8;;
    int beatidx = (int)(fbeat);
    float beatoffset = fbeat - beatidx;
    if     (beatidx == 0) envelope[i] = st->beat0;
    else if(beatidx == 1) envelope[i] = st->beat1;
    else if(beatidx == 2) envelope[i] = st->beat2;
    else if(beatidx == 3) envelope[i] = st->beat3;
    else if(beatidx == 4) envelope[i] = st->beat4;
    else if(beatidx == 5) envelope[i] = st->beat5;
    else if(beatidx == 6) envelope[i] = st->beat6;
    else if(beatidx == 7) envelope[i] = st->beat7;
    if (beatoffset > st->envelope[0]) {
      envelope[i] *= POW(MAX(st->envdcy_min, st->envelope[1]), (beatoffset - st->envelope[0]));
    }
  }
}

void wavseq_proc(wavseq_state* st, float* out[], float* in[], float* speed, double* position)
{
  int i;
  int frame_size = st->frame_size;
  int beatlen = st->buflen / st->bar_max / 8; // 8beat;
  int banksel = st->banksel;

  VARDECLR(float*, in_rec);
  VARDECLR(float*, temp);
  VARDECLR(double, position_adj);
  VARDECLR(float, envelope);
  SAVE_STACK;
  ALLOC(temp,      2, float*);
  ALLOC(in_rec,    2, float*);
  ALLOC(in_rec[0], frame_size, float);
  ALLOC(in_rec[1], frame_size, float);
  ALLOC(temp[0],   frame_size, float);
  ALLOC(temp[1],   frame_size, float);
  ALLOC(position_adj, frame_size, double);
  ALLOC(envelope,  frame_size, float);

  /* Change bank if available */
  if (banksel != st->banksel_) {
    if (!st->tdvsb[banksel]) {
      banksel = st->banksel_;
    }
    else {
      st->banksel_ = banksel;
    }
  }

  /* Compute buffer length in number of 8beat */
  int buflen = FBvsb_get_buflen(st->fdvsb[banksel]);
  int beatnum = (int)(EXP2(((int)(LOG2((float)buflen / (float)beatlen) + 0.5f))));
  
  /* Compute bar length */
  float barlen = beatnum / 8.f;

  /* Adjust recording gain */
  if (st->recgain > st->recgain_) {
    st->rec_startspeed = speed[0];
    st->rec_len = 0.f;
    st->recend = 0;
  }
  if (st->recgain > 0.f && st->rec_startspeed == 0.f) {
    st->rec_startspeed = speed[0];
  }
  if (st->rec_startspeed * speed[0] < 0.f) {
    st->recend = 1;
  }
  if (FABS(st->rec_len) >= barlen) {
    st->recend = 1;
  }
  if (st->recend) {
    st->recgain *= st->recfade;
    if (st->recgain < dBtoM(-40.f)) st->recgain = 0.f;
  }
  st->recgain_ = st->recgain;

  vec_muls(in_rec[0], in[0], st->recgain, frame_size);
  vec_muls(in_rec[1], in[1], st->recgain, frame_size);

  /* overdub setting */
  if (st->recgain == 1.f) {
    vsb_set_feedbackgain(st->tdvsb[banksel], 0.f);
    FBvsb_set_feedbackgain(st->fdvsb[banksel], 0.f);
  }
  else {
    vsb_set_feedbackgain(st->tdvsb[banksel], 1.f);
    FBvsb_set_feedbackgain(st->fdvsb[banksel], 1.f);
  }

  /* Adjust position for selected bank */
  COPY(position_adj, position, frame_size);
  for (i=0; i<frame_size; i++) {
    ADJIDX(position_adj[i], 0, (double)beatnum/8.0, (double)beatnum/8.0);
    position_adj[i] = position_adj[i] / ((double)beatnum/8.0);
  }
  st->trackpos = position_adj[frame_size-1];

  /* TDVSB */
  vsb_process(st->tdvsb[banksel], out, in_rec, speed, position_adj, frame_size);

  /* FDVSB */
  FBvsb_process(st->fdvsb[banksel], temp, in_rec, speed, position_adj);

  /* envelope */
  compute_envelope(st, envelope, position);

  /* Update position */
  double progress = position[frame_size-1] - st->lastpos;
  if (FABS(progress) > st->bar_max/2) {
    if (progress < 0.f) progress += st->bar_max;
    else progress -= st->bar_max;
  }
  if (!st->recend) {
    st->rec_len += (float)progress;
  }
  st->lastpos = position[frame_size-1];

  /* Balance between TD vs FD */
  vec_wadd1(out[0], 1.0f - st->fdbal, st->fdbal, temp[0], frame_size);
  vec_wadd1(out[1], 1.0f - st->fdbal, st->fdbal, temp[1], frame_size);

  /* Apply outgain and envelope*/
  vec_mul1s(envelope, st->outgain, frame_size);
  vec_mul1(out[0], envelope, frame_size);
  vec_mul1(out[1], envelope, frame_size);  

  RESTORE_STACK;
}

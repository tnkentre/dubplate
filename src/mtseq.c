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
 * @file mtseq.c
 * @brief mtseq processing
 * @version $LastChangedRevision:$
 * @author Ryo Tanaka
 */
#include "yavc.h"
#include "midi.h"
#include "vectors.h"
#include "tcanalysis.h"
#include "vsb.h"
#include "FBvsb.h"
#include "CircularBuf.h"
#include "wavseq.h"
#include "mtseq.h"

#define TRACK_NUM         (4)
#define BAR_MAX           (4)
#define MIDICLOCK_AVGNUM  (96)

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

/*
 * mtseq functions
 */
/* mtseq State */
struct mtseq_state_ {
  AC4_HEADER;

  /* Configuration */
  int fs;
  int frame_size;
  float bpm;
  double pos;
  float fpos;
  float barpos;
  int delaylen;
  int delaysel;
  int delayadj;
  uint64_t midiclock_lastcnt;
  uint64_t midiclock_cnt;
  int midiclock_duration[MIDICLOCK_AVGNUM];
  int midiclock_idx;

  /* Audio Components */
  TCANALYSIS_State * tcana;
  wavseq_state * wavseq[TRACK_NUM];
  SimpleCBState * delay[2];
  MIDI_State* midi;
} ;

static const RegDef_t rd[] = {
  AC_REGDEF(bpm,       CLI_ACFPTM, mtseq_state, "BPM"),
  AC_REGDEF(fpos,      CLI_ACFPTM, mtseq_state, "fpos"),
  AC_REGDEF(barpos,    CLI_ACFPTM, mtseq_state, "barpos"),
  AC_REGDEF(delaysel,  CLI_ACIPTM, mtseq_state, "delay selection"),
  AC_REGDEF(delayadj,  CLI_ACIPTM, mtseq_state, "delay adjustment [range: 0 - 512]"),
};

mtseq_state* mtseq_init(const char *name, int fs, int frame_size, MIDI_State* midi)
{
  int i;
  mtseq_state* st;
  char subname[32];
  
  st = MEM_ALLOC(MEM_SDRAM, mtseq_state, 1, 4);
  AC4_ADD(name, st, "mtseq");
  AC4_ADD_REG(rd);

  st->fs         = fs;
  st->frame_size = frame_size;
  st->midi       = midi;

  st->bpm        = 120;
  st->delaylen   = st->fs * 10;
  st->delayadj   = 270;

  sprintf(subname, "%s_tcana",name);
  st->tcana = tcanalysis_init(subname, st->fs, 1000.f);

  for (i=0; i<TRACK_NUM; i++) {
    sprintf(subname, "%s_seq%d", name, i);
    st->wavseq[i] = wavseq_init(subname, st->fs, st->frame_size, BAR_MAX);
  }

  for (i=0; i<2; i++) {
    st->delay[i] = simplecb_init(st->delaylen + st->frame_size);
  }

  yavc_osc_add_acreg((char*)name, "bpm");
  yavc_osc_add_acreg((char*)name, "barpos");
  yavc_osc_add_acreg((char*)name, "delaysel");

  return st;
}

void mtseq_proc(mtseq_state* st, float* out[], float* in[], float* tc[])
{
  int i,j;
  int frame_size = st->frame_size;

  VARDECLR(float*, out_track);
  VARDECLR(float, speed);
  VARDECLR(double, position);
  SAVE_STACK;
  ALLOC(out_track, 2, float*);
  ALLOC(out_track[0], frame_size, float);
  ALLOC(out_track[1], frame_size, float);
  ALLOC(speed,        frame_size, float);
  ALLOC(position,     frame_size, double);

  /* Receive MIDI */
  int msg_num = midi_recv_num(st->midi);
  const MIDI_MSG* msg = midi_recv_buffer(st->midi);
  for (i=0; i<msg_num; i++) {
    //TRACE(LEVEL_INFO, "MIDI:%02X %02X %02X", msg[i].data[0], msg[i].data[1], msg[i].data[2]);
    if (msg[i].data[0] == 0xF8) {
      st->midiclock_duration[st->midiclock_idx] = (int)(st->midiclock_cnt + msg[i].sample_offset - st->midiclock_lastcnt);
      st->midiclock_idx = (st->midiclock_idx + 1) % MIDICLOCK_AVGNUM;
      st->midiclock_lastcnt = st->midiclock_cnt + msg[i].sample_offset;
      int sum = 0.f;
      for (j=0; j<MIDICLOCK_AVGNUM; j++) {
        sum += st->midiclock_duration[j];
      }
      st->bpm = (int)(60.f * st->fs * MIDICLOCK_AVGNUM / 24 / (float)sum + 0.5f);
    }
  }
  st->midiclock_cnt += frame_size;

  /* Compute speed from TC sound */
  tcanalysis_process(st->tcana, speed, tc[0], tc[1], frame_size);

  /* Create position buffer */
  double step = st->bpm / 60.f / st->fs / 4.f;
  for (i=0; i<frame_size; i++) {
    position[i] = st->pos;
    st->pos += step * speed[i];
    ADJIDX(st->pos, 0.0, BAR_MAX, BAR_MAX);
  }
  st->fpos = (float)(st->pos);
  st->barpos = st->fpos - (int)(st->fpos);

  ZERO(out[0], frame_size);
  ZERO(out[1], frame_size);
  for (i=0; i<TRACK_NUM; i++) {
    wavseq_proc(st->wavseq[i],  out_track, in, speed, position);
    vec_add1(out[0], out_track[0], frame_size);
    vec_add1(out[1], out_track[1], frame_size);
  }

  RESTORE_STACK;
}

void mtseq_delay(mtseq_state* st, float* out[], float* in[])
{
  int i;
  for (i=0; i<2; i++) {
    simplecb_write(st->delay[i], in[i], st->frame_size);
  }

  if (st->bpm <= 0.f) {
    for (i=0; i<2; i++) {
      ZERO(out[i], st->frame_size);
    }
    return;
  }

  float delaytime_bar = 1.f;
  if      (st->delaysel == 0) delaytime_bar = 1.f / 32.f;
  else if (st->delaysel == 1) delaytime_bar = 1.f / 16.f;
  else if (st->delaysel == 2) delaytime_bar = 1.f / 8.f;
  else if (st->delaysel == 3) delaytime_bar = 1.5f / 8.f;
  else if (st->delaysel == 4) delaytime_bar = 2.f / 8.f;
  else if (st->delaysel == 5) delaytime_bar = 3.f / 8.f;
  else if (st->delaysel == 6) delaytime_bar = 4.f / 8.f;
  else if (st->delaysel == 7) delaytime_bar = 1.f;
  float delaytap = 60.f / st->bpm * 4 * delaytime_bar * st->fs;
  while(delaytap > st->delaylen) delaytap /= 2.f;
  delaytap -= 2 * st->frame_size + st->delayadj;

  for (i=0; i<2; i++) {
    simplecb_read(st->delay[i], out[i], st->frame_size, delaytap);
  }
}
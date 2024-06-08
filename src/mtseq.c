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
#include "vectors.h"
#include "tcanalysis.h"
#include "vsb.h"
#include "FBvsb.h"
#include "wavseq.h"
#include "mtseq.h"

#define TRACK_NUM (4)

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

  /* Audio Components */
  TCANALYSIS_State * tcana;
  wavseq_state * wavseq[TRACK_NUM];
} ;

static const RegDef_t rd[] = {
  AC_REGDEF(bpm,      CLI_ACFPTM, mtseq_state, "BPM"),
};

mtseq_state* mtseq_init(const char *name, int fs, int frame_size)
{
  int i;
  mtseq_state* st;
  char subname[32];
  
  st = MEM_ALLOC(MEM_SDRAM, mtseq_state, 1, 4);
  AC4_ADD(name, st, "mtseq");
  AC4_ADD_REG(rd);

  st->fs         = fs;
  st->frame_size = frame_size;

  sprintf(subname, "%s_tcana",name);
  st->tcana = tcanalysis_init(subname, st->fs, 1000.f);

  for (i=0; i<TRACK_NUM; i++) {
    sprintf(subname, "%s_seq%d", name, i);
    st->wavseq[i] = wavseq_init(subname, st->fs, st->frame_size);
    if (!i) yavc_osc_add_acreg((char*)name, "trackpos");
  }

  return st;
}

void mtseq_proc(mtseq_state* st, float* out[], float* in[], float* tc[])
{
  int i;
  int frame_size = st->frame_size;

  VARDECLR(float*, out_track);
  VARDECLR(float, speed);
  SAVE_STACK;
  ALLOC(out_track, 2, float*);
  ALLOC(out_track[0], frame_size, float);
  ALLOC(out_track[1], frame_size, float);
  ALLOC(speed,        frame_size, float);

  /* Compute speed from TC sound */
  tcanalysis_process(st->tcana, speed, tc[0], tc[1], frame_size);

  ZERO(out[0], frame_size);
  ZERO(out[1], frame_size);
  for (i=0; i<TRACK_NUM; i++) {
    wavseq_proc(st->wavseq[i],  out_track, in, speed);
    vec_add1(out[0], out_track[0], frame_size);
    vec_add1(out[1], out_track[1], frame_size);
  }

  RESTORE_STACK;
}

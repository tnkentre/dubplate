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
 * @file turntable.c
 * @brief Turntable processing
 * @version $LastChangedRevision:$
 * @author Ryo Tanaka
 */
#include "yavc.h"
#include "vectors.h"
#include "tcanalysis.h"
#include "vsb.h"
#include "FBvsb.h"
#include "turntable.h"

/*
 * TurnTable functions
 */
/* Turntable State */
struct TurntableState_ {
  AC4_HEADER;

  /* Configuration */
  int flags;
  int fs;
  int frame_size;

  /* Parameters */
  float ingain;
  float recgain;
  float recgain_;
  int   recend;
  float recfade;
  float overdub;
  float fdbal;
  float monitor;
  int   looplen;
  int   looplen_;
  float loopposin;
  float looppos;
  int   loopidx;
  int   loopsize;
  int   size;
  int   ipos;
  float fpos;
  int   rec_startpos;
  float rec_startspeed;

  /* for OSC */
  int xy_len;
  float* vinyl;
  float* loopstart;
  float* loopend;

  /* Audio Components */
  TCANALYSIS_State * tcana;
  VSB_State        * tdvsb;
  FBVSB_State      * fdvsb;
} ;

static const RegDef_t rd[] = {
  AC_REGDEF(flags,    CLI_ACXPTM, TurntableState, "Control flags"),
  AC_REGDEF(ingain,   CLI_ACFPTM, TurntableState, "Input gain"),
  AC_REGDEF(recgain,  CLI_ACFPTM, TurntableState, "Recording gain"),
  AC_REGDEF(recfade,  CLI_ACFPTM, TurntableState, "Fade out of recording"),
  AC_REGDEF(overdub,  CLI_ACFPTM, TurntableState, "overdub"),
  AC_REGDEF(fdbal,    CLI_ACFPTM, TurntableState, "Balance to FD"),
  AC_REGDEF(monitor,  CLI_ACFPTM, TurntableState, "Input monitor level"),
  AC_REGDEF(looplen,  CLI_ACIPTM, TurntableState, "loop length"),
  AC_REGDEF(loopposin,CLI_ACFPTM, TurntableState, "loop position"),
  AC_REGDEF(looppos,  CLI_ACFPTM, TurntableState, "loop position"),
  AC_REGDEF(ipos,     CLI_ACIPTM, TurntableState, "track position"),
  AC_REGDEF(fpos,     CLI_ACFPTM, TurntableState, "track position"),
  AC_REGADEF(vinyl,     xy_len, CLI_ACFPTMA,   TurntableState, "position"),
  AC_REGADEF(loopstart, xy_len, CLI_ACFPTMA,   TurntableState, "loop start position"),
  AC_REGADEF(loopend,   xy_len, CLI_ACFPTMA,   TurntableState, "loop start position"),
};

TurntableState* Turntable_init(const char *name, int fs, int frame_size)
{
  TurntableState* st;
  char subname[32];
  float bar = 2.0f;
  float sec = 60.f / 33.3f * bar;
  
  st = MEM_ALLOC(MEM_SDRAM, TurntableState, 1, 4);
  AC4_ADD(name, st, "Turntable");
  AC4_ADD_REG(rd);

  st->fs         = fs;
  st->frame_size = frame_size;

  st->ingain     = 1.0f;
  st->recgain    = 0.0f;
  st->recfade    = dBtoM(-6.0 / (0.02f * fs) * frame_size);
  st->overdub    = 0.0f;
  st->fdbal      = 0.0f;
  st->monitor    = 1.0f;
  st->looplen    = 4;

  st->xy_len  = 2;
  st->vinyl     = MEM_ALLOC(MEM_SDRAM, float, st->xy_len, 4);
  st->loopstart = MEM_ALLOC(MEM_SDRAM, float, st->xy_len, 4);
  st->loopend   = MEM_ALLOC(MEM_SDRAM, float, st->xy_len, 4);

  sprintf(subname, "%s_tcana",name);
  st->tcana = tcanalysis_init(subname, st->fs, 1000.f);

  sprintf(subname, "%s_fdvsb",name);    
  st->fdvsb = FBvsb_init(subname, 2, st->fs, st->frame_size, sec);

  st->size = FBvsb_get_buflen(st->fdvsb);

  sprintf(subname, "%s_tdvsb",name);    
  st->tdvsb = vsb_init(subname, st->size);

  st->loopidx  = 0;
  st->loopsize   = st->size;

  return st;
}

void Turntable_proc(TurntableState* st, float* out[], float* in[], float* tc[])
{
  int frame_size = st->frame_size;
  int beatsize = st->size / 8;

  VARDECLR(float, speed);
  VARDECLR(float*, in_adj);
  VARDECLR(float*, in_rec);
  VARDECLR(float*, temp);
  SAVE_STACK;
  ALLOC(speed, frame_size, float);
  ALLOC(in_adj, 2, float*);
  ALLOC(in_rec, 2, float*);
  ALLOC(temp,   2, float*);
  ALLOC(in_adj[0],  frame_size, float);
  ALLOC(in_adj[1],  frame_size, float);
  ALLOC(in_rec[0],  frame_size, float);
  ALLOC(in_rec[1],  frame_size, float);
  ALLOC(temp[0],  frame_size, float);
  ALLOC(temp[1],  frame_size, float);

  /* loop length */
  if (st->looplen != st->looplen_) {
    switch(st->looplen) {
    case 0:
      st->loopsize = beatsize;
      break;
    case 1:
      st->loopsize = 2 * beatsize;
      break;
    case 2:
      st->loopsize = 3 * beatsize;
      break;
    case 3:
      st->loopsize = 4 * beatsize;
      break;
    default:
    case 4:
      st->loopsize = 8 * beatsize;
      break;
    }
    /* get the current beat and set looppos */
    st->looppos = (int)(vsb_get_pos(st->tdvsb) / beatsize) / 8.f;
    st->looplen_ = st->looplen;
  }
  /* loop posistion */
  if (st->loopposin >= 0.f && st->loopposin <= 1.f) {
    st->loopidx = (int)((st->loopposin) * 8) * (st->size / 8);
  }
  st->looppos = (float)(st->loopidx) / (st->size) + 0.0625f;

  /* loop region */
  float pos;
  st->loopstart[0] = 10.f * RECIP(30.48f);
  st->loopstart[1] = (float)(st->loopidx) / (st->size);
  pos = (float)(st->loopidx + st->loopsize) / (st->size); if (pos > 1.0) pos-= 1.f;
  st->loopend[0] = 10.f * RECIP(30.48f);
  st->loopend[1] = pos;

  /* update loop */
  vsb_set_loop(st->tdvsb, st->loopidx, st->loopsize);
  FBvsb_set_loop(st->fdvsb, st->loopidx, st->loopsize);

  /* Compute speed from TC sound */
  tcanalysis_process(st->tcana, speed, tc[0], tc[1], frame_size);

  /* auto recgain control */
  if (st->overdub == 0.f) {
    if (st->recgain > st->recgain_) {
      st->rec_startpos   = st->ipos;
      st->rec_startspeed = speed[0];
      st->recend = 0;
    }
    if (st->recgain > 0.f && st->rec_startspeed == 0.f) {
      st->rec_startspeed = speed[0];
    }
    if (st->rec_startspeed * speed[0] < 0.f) {
      st->recend = 1;
    }
    if (st->rec_startspeed > 0.f && st->ipos >= st->rec_startpos + st->loopsize) {
      st->recend = 1;
    }
    if (st->rec_startspeed < 0.f && st->ipos <= st->rec_startpos - st->loopsize) {
      st->recend = 1;
    }
    if (st->recend) {
      st->recgain *= st->recfade;
      if (st->recgain < dBtoM(-40.f)) st->recgain = 0.f;
    }
  }
  st->recgain_ = st->recgain;

  /* overdub setting */
  if (st->overdub == 0.f && st->recgain == 1.f) {
    vsb_set_feedbackgain(st->tdvsb, 0.f);
    FBvsb_set_feedbackgain(st->fdvsb, 0.f);
  }
  else {
    vsb_set_feedbackgain(st->tdvsb, 1.f);
    FBvsb_set_feedbackgain(st->fdvsb, 1.f);
  }

  /* Adjust gain */
  vec_muls(in_adj[0], in[0], st->ingain, frame_size);
  vec_muls(in_adj[1], in[1], st->ingain, frame_size);
  vec_muls(in_rec[0], in_adj[0], st->recgain, frame_size);
  vec_muls(in_rec[1], in_adj[1], st->recgain, frame_size);

  /* TDVSB */
  vsb_process(st->tdvsb, out, in_rec, speed, NULL, frame_size);

  /* FDVSB */
  FBvsb_process(st->fdvsb, temp, in_rec, speed, NULL);

  /* Balance between TD vs FD */
  vec_wadd1(out[0], 1.0f - st->fdbal, st->fdbal, temp[0], frame_size);
  vec_wadd1(out[1], 1.0f - st->fdbal, st->fdbal, temp[1], frame_size);

  /* compute vinyl level */
  vec_mul(temp[0], out[0], out[0], frame_size);
  vec_mul(temp[1], out[1], out[1], frame_size);
  float vinyl_lev;
  vinyl_lev =                0.5f * MtodB(vec_sum(temp[0], frame_size) * RECIP((float)frame_size));
  vinyl_lev = MAX(vinyl_lev, 0.5f * MtodB(vec_sum(temp[1], frame_size) * RECIP((float)frame_size)));

  /* Mix input */
  vec_wadd1(out[0], 1.0f, MAX(st->monitor, st->recgain), in_adj[0], frame_size);
  vec_wadd1(out[1], 1.0f, MAX(st->monitor, st->recgain), in_adj[1], frame_size);

  /* update position */
  st->fpos += vec_sum(speed, frame_size);
  st->ipos += (int)(st->fpos);
  st->fpos -= (float)((int)(st->fpos));

  /* Update vinyl */
  float base  = 10.f * RECIP(30.48f);
  float maxdb =   0.0f;
  float mindb =  -50.0f; 
  st->vinyl[0] = MAX(0.f, MIN(1.f, (vinyl_lev - mindb) * RECIP(maxdb - mindb))) * (1-base) + base;
  st->vinyl[1] = vsb_vinylpos(st->tdvsb);

  RESTORE_STACK;
}

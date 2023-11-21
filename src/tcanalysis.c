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
 * @file tcanalysis.c
 * @brief This file implements a module to analyze TC input like tone on Serato TC vinyl
 * @version $LastChangedRevision:$
 * @author Ryo Tanaka
 */
#include "yavc.h"
#include "vectors.h"
#include "tcanalysis.h"

struct TCANALYSIS_State_ {
  AC4_HEADER;
  /* configuration and history */
  float fs;
  float zcr_base;
  float direction;
  float speed;
  float speed_avg;
  float tcL;
  float tcR;
  int   zcr_L;
  int   zcr_R;

  /* parameters */
  float level_thld;
  float level;
  float level_avg;
  float speed_alpha;
} ;

static const RegDef_t rd[] = {
  AC_REGDEF(level_thld,   CLI_ACFPTM,   TCANALYSIS_State, "Level threshold to detect input signal."),
  AC_REGDEF(level,        CLI_ACFPTM,   TCANALYSIS_State, "Actual level of input"),
  AC_REGDEF(level_avg,    CLI_ACFPTM,   TCANALYSIS_State, "Average level of input"),
  AC_REGDEF(speed,        CLI_ACFPTM,   TCANALYSIS_State, "Speed"),
  AC_REGDEF(speed_avg,    CLI_ACFPTM,   TCANALYSIS_State, "Speed(smooth)"),
  AC_REGDEF(speed_alpha,  CLI_ACFPTM,   TCANALYSIS_State, "Speed alpha"),
};

TCANALYSIS_State *tcanalysis_init(const char * name, float fs, float freq_tc)
{
  TCANALYSIS_State* st;
  st = MEM_ALLOC(MEM_SDRAM, TCANALYSIS_State, 1, 8);

  AC4_ADD(name, st, "TC analysis");
  AC4_ADD_REG(rd);
  
  st->fs         = fs;
  st->zcr_base   = fs / freq_tc;
  st->direction  = 0.0f;
  st->speed      = 0.0f;
  st->tcL        = 0.0f;
  st->tcR        = 0.0f;
  st->zcr_L      = -1;
  st->zcr_R      = -1;

  st->level_thld = -40.0f;
  st->level      = -100.0f;
  st->level_avg  = -100.0f;
  st->speed_alpha = 0.75f;

  return st;
}

void tcanalysis_process(TCANALYSIS_State * restrict st, float* speed, float* tcL, float* tcR, int len)
{
  int i;
  for (i=0; i<len; i++) {
    st->level = 10.0f * LOG10(tcL[i]*tcL[i] + tcR[i]*tcR[i]);
    st->level_avg = st->level + 0.999999f * (st->level_avg - st->level);
    if (st->level < st->level_thld) {
      /* No input signal detected. Reset the internal parameters. */
      st->zcr_L     = -1;
      st->zcr_R     = -1;
      st->direction = 0.0f;
      st->speed     = 0.0f;
      st->tcL       = 0.0f;
      st->tcR       = 0.0f;
    }
    else {
      /* Increment zero-crossing counter when the first crossing has been detected. */
      if (st->zcr_L >= 0) st->zcr_L++;
      if (st->zcr_R >= 0) st->zcr_R++;
      /* Detect zero-crossing on the Lch signal. */
      if (st->tcL * tcL[i] <= 0.0f && st->tcL!=0.0f) {
	/* Calculate current speed when zero-crossing is detected. */
	if (tcL[i] > 0.0f){
	  if (st->zcr_L > 0)	st->speed = st->zcr_base / (float)st->zcr_L;
	  st->zcr_L = 0;
	}
	/* Encode the direction */
	if      (tcL[i] * tcR[i] > 0.0f) st->direction =  1.0f;
	else if (tcL[i] * tcR[i] < 0.0f) st->direction = -1.0f;
	else                             st->direction =  0.0f;
      }
      /* Detect zero-crossing on the Lch signal. */
      if (st->tcR * tcR[i] <= 0.0f && st->tcR!=0.0f) {
	/* Calculate current speed when zero-crossing is detected. */
	if (tcR[i] > 0.0f){
	  if (st->zcr_R > 0) st->speed = st->zcr_base / (float)st->zcr_R;
	  st->zcr_R = 0;
	}
	/* Encode the direction */
	if      (tcL[i] * tcR[i] < 0.0f) st->direction =  1.0f;
	else if (tcL[i] * tcR[i] > 0.0f) st->direction = -1.0f;
	else                             st->direction =  0.0f;
      }
      /* Store the latest sample value */
      st->tcL = tcL[i];
      st->tcR = tcR[i];
    }
    /* speed average */
    st->speed_avg = st->speed_alpha * st->speed_avg + (1.f - st->speed_alpha) * st->speed * st->direction;
    
    /* Put the result */
    speed[i] = st->speed_avg;
  }
}

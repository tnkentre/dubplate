/******************************************************************************
 *
 * TANAKA ENTERPRISES CONFIDENTIAL
 * __________________
 *
 *  [2023] Tanaka Enterprises
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
 * @file dsptop.c
 * @brief This is a template to explain how to use the YAVC frame work
 * @version $LastChangedRevision:$
 * @author Ryo Tanaka
 */

#include "yavc.h"
#include "midi.h"
#include "vectors.h"
#include "vectors_cplx.h"
#include "FB.h"
#include "FB_filters.h"
//#include "turntable.h"
#include "mtseq.h"

#include "dsptop.h"



//----------------------------------------------------------
// DSPTOP definitions
//----------------------------------------------------------
typedef enum {
  CH_IN_AUDL = 0,
  CH_IN_AUDR,
  CH_IN_TC0L,
  CH_IN_TC0R,
  CH_IN_TC1L,
  CH_IN_TC1R,
  CH_IN_EFXL,
  CH_IN_EFXR,
  CH_IN_NUM
} CH_IN;

typedef enum {
  CH_OUT_MIXL = 0,
  CH_OUT_MIXR,
  CH_OUT_AUD0L,
  CH_OUT_AUD0R,
  CH_OUT_AUD1L,
  CH_OUT_AUD1R,
  CH_OUT_EFXL,
  CH_OUT_EFXR,
  CH_OUT_NUM
} CH_OUT;

#define NTURNTABLE   (1)
//----------------------------------------------------------
// DSPTOP configuration (Needed for DspCoreAC class)
//----------------------------------------------------------
const char dsptop_name[]      = "dubplate";
const int  dsptop_chnum_in    = CH_IN_NUM;
const int  dsptop_chnum_out   = CH_OUT_NUM;

//----------------------------------------------------------
// DSPTOP State
//----------------------------------------------------------
struct dsptop_state_{
  AC4_HEADER;

  /* Configuration */
  int fs;
  int frame_size;
  int nch_input;
  int nch_output;

  /* parameters */
  float bpm;
  int source;

  /* Co-components */
  mtseq_state* mtseq[NTURNTABLE];

  /* MIDI support */
  MIDI_State*     midi;
} ;

//----------------------------------------------------------
// DSPTOP Parameters
//----------------------------------------------------------
static const RegDef_t rd[] = {
  AC_REGDEF(bpm,     CLI_ACFPTM,   dsptop_state, "bpm [range: 20.0   - 200.0  ]"),
  AC_REGDEF(source,  CLI_ACIPTM,   dsptop_state, "source [range: 0   - 1  ]"),
};

//**********************************************************
// Interface
//**********************************************************
//----------------------------------------------------------
// DSPTOP initializing
//----------------------------------------------------------
dsptop_state* dsptop_init(int fs, int frame_size)
{
  int i;
  dsptop_state* st;
  char name[32];

  /* Initialize YAVC frame work */
  yavc_init_memspecified(128*1024, NULL, 0, NULL, 0, NULL, 0);
  yavc_mod_init(); 

  /* Create instance of dsptop */
  st = MEM_ALLOC(MEM_HEAP, dsptop_state, 1, 4);
  AC4_ADD("top", st, "The top module");
  AC4_ADD_REG(rd);

  /* Initialize the members */
  st->fs         = fs;
  st->frame_size = frame_size;
  st->nch_input  = CH_IN_NUM;
  st->nch_output = CH_OUT_NUM;

  st->bpm = 130;

  /* Create MIDI state */
  st->midi        = midi_init("midi", st->frame_size);

  /* Initialize Co-components */
  for (i=0; i<NTURNTABLE; i++) {
    sprintf(name, "mt%d",i);
    st->mtseq[i] = mtseq_init(name, st->fs, st->frame_size, st->midi);
  }

  yavc_osc_set_moninterval(20);

  /* Associate this initialized top */
  yavc_mod_associate(st);

  /* Store to static */
  return st;
}

//----------------------------------------------------------
// DSPTOP processing
//----------------------------------------------------------
void dsptop_proc(dsptop_state* st, float* out[], float* in[])
{
  Task_startNewCycle(); // This is called to compute CPU load

  float ** src;

  if (st->source) src = &in[CH_IN_EFXL];
  else            src = &in[CH_IN_AUDL];
  mtseq_proc(st->mtseq[0], &out[CH_OUT_AUD0L], src, &in[CH_IN_TC0L]);
//  mtseq_proc(st->mtseq[1], &out[CH_OUT_AUD1L], src, &in[CH_IN_TC1L]);

  vec_add(out[CH_OUT_MIXL], out[CH_OUT_AUD0L], out[CH_OUT_AUD1L], st->frame_size);
  vec_add(out[CH_OUT_MIXR], out[CH_OUT_AUD0R], out[CH_OUT_AUD1R], st->frame_size);

  mtseq_delay(st->mtseq[0], &out[CH_OUT_EFXL], &in[CH_IN_EFXL]);

}
void dsptop_proc_with_midi(dsptop_state* st, float* out[], float* in[],
  MIDI_MSG* midirecv_msg, size_t midirecv_len,
  const MIDI_MSG** midisend_msg, size_t* midisend_len)
{
  /* Receive MIDI data */
  midi_start_frame(st->midi, (MIDI_MSG*)midirecv_msg, midirecv_len);

  dsptop_proc(st, out, in);

  /* send MIDI data to host */
  *midisend_msg = midi_send_buffer(st->midi);
  *midisend_len = midi_send_num(st->midi);
}

//----------------------------------------------------------
// DSP closing
//----------------------------------------------------------
void dsptop_close(dsptop_state* st)
{  
  yavc_mod_close(st);
}

//----------------------------------------------------------
// DSP parameter symbol table
//----------------------------------------------------------
SymbolTable* dsptop_symboltable(dsptop_state* top)
{
  return yavc_mod_get_symboltable(top);
}

#ifndef WITH_JACK
#undef BUILD_JACK
#endif
#if defined(BUILD_JACK)
//----------------------------------------------------------
// Main function for standalone app
//----------------------------------------------------------
/* Audio support with Jack */
#include "jack_fwk.h"
int main(int argc, char* argv[])
{
  /* yavc_init() needs to be called prior to dsptop_init() for jack */
  yavc_init();

  /* Create jack instance */
  jack_fwk_state* jack = jack_fwk_init("jack",   CH_IN_NUM,   CH_OUT_NUM);

  /* Create dsptop instance */
  dsptop_state* dsptop = dsptop_init(jack_fwk_get_fs(jack), jack_fwk_get_frame_size(jack));

  /* Start audio process with jack */
  jack_fwk_start(jack, (JACK_FWK_PROCESS_CALLBACK)dsptop_proc, dsptop);

  /* CLI */
  char cmd[128];
  while(1) {
    printf("%s> ", dsptop->ste->name);
    if(fgets(cmd, sizeof(cmd), stdin)) {
      if (!strncmp("exit", cmd, 4)) break;
      if (!strncmp("dsp ", cmd, 4)) CLI_COMMAND(&cmd[4]);
    }
  }

  jack_fwk_close(jack);

  dsptop_close(dsptop);
}

#elif defined(BUILD_WAVIO)

//----------------------------------------------------------
// Main function for standalone app
//----------------------------------------------------------
/* Audio support with wav file IO */
#include "wav.h"
int main(int argc, char* argv[])
{
  int i;

  if (argc!=3) {
    printf("Usage: %s_wavio [input file] [output file]\n", dsptop_name);
    return 0;
  }

  /* Create dsptop instance */
  dsptop_state* dsptop = dsptop_init(48000, 512);

  /* Create wav IO */
  WavState* wav_in  = wav_init("wavin", dsptop->fs);
  WavState* wav_out = wav_init("wavout", dsptop->fs);

  /* Open files */
  wav_open_FileRead(wav_in, argv[1]);
  wav_open_FileWrite(wav_out, argv[2], dsptop->fs, CH_OUT_NUM, 32);

  /* Prepare IO buffer */
  VARDECL(float*, in);
  VARDECL(float*, out);
  SAVE_STACK;
  ALLOC(in, CH_IN_NUM, float*);
  ALLOC(out, CH_OUT_NUM, float*);
  for (i=0; i<CH_IN_NUM; i++) ALLOC(in[i], dsptop->frame_size, float);
  for (i=0; i<CH_OUT_NUM; i++) ALLOC(out[i], dsptop->frame_size, float);
  
  /* Main process */
  int nsample;
  do {

    /* Clear buffer */
    for (i=0; i<CH_IN_NUM; i++) ZERO(in[i], dsptop->frame_size);
    for (i=0; i<CH_OUT_NUM; i++) ZERO(out[i], dsptop->frame_size);

    /* Read data from wav file */
    nsample = wav_read(wav_in, in, dsptop->frame_size);

    /* dsptop process */
    dsptop_proc(dsptop, out, in);

    /* Write data to wav file */
    wav_write(wav_out, (const float**)out, nsample);
    
  } while(nsample);

  RESTORE_STACK;

  wav_close(wav_in);
  wav_close(wav_out);

  dsptop_close(dsptop);
}
#endif
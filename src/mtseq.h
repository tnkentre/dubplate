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
 * @file mtseq.h
 * @brief Audio processing
 * @version $LastChangedRevision:$
 * @author Ryo Tanaka
 */

#ifndef _mtseq_H_
#define _mtseq_H_

/** Internal mtseq state. Should never be accessed directly. */
struct mtseq_state_;

/** @class mtseq_state
 * This holds the state of the FBbasssynth.
 */
typedef struct mtseq_state_ mtseq_state;

/** @fn mtseq_state* mtseq_init(const char *name, int fs, int frame_size)
 * @brief This function creates a mtseq component
 * @param name Name of the component
 * @param fs Sampling frequency
 * @param frame_size Number of frame size
 * @return Initialized state of the mtseq
 */
mtseq_state* mtseq_init(const char *name, int fs, int frame_size, MIDI_State* midi);

/** @fn void mtseq_proc(mtseq_state* st, float* out[], float* in[], float* tc[])
 * @brief This function execute mtseq
 * @param st State of the mtseq
 * @param out Output buffer's pointer array
 * @param in  Input audio buffer's pointer array
 * @param tc  Input time code buffer's pointer array
 */
void mtseq_proc(mtseq_state* st, float* out[], float* in[], float* tc[]);

void mtseq_delay(mtseq_state* st, float* out[], float* in[]);

#endif /* _mtseq_H_ */

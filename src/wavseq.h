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
 * @file wavseq.h
 * @brief Audio processing
 * @version $LastChangedRevision:$
 * @author Ryo Tanaka
 */

#ifndef _wavseq_H_
#define _wavseq_H_

/** Internal wavseq state. Should never be accessed directly. */
struct wavseq_state_;

/** @class wavseq_state
 * This holds the state of the FBbasssynth.
 */
typedef struct wavseq_state_ wavseq_state;

/** @fn wavseq_state* wavseq_init(const char *name, int fs, int frame_size)
 * @brief This function creates a wavseq component
 * @param name Name of the component
 * @param fs Sampling frequency
 * @param frame_size Number of frame size
 * @return Initialized state of the wavseq
 */
wavseq_state* wavseq_init(const char *name, int fs, int frame_size, int bar_max);

/** @fn void wavseq_proc(wavseq_state* st, float* out[], float* in[], float* tc[])
 * @brief This function execute wavseq
 * @param st State of the wavseq
 * @param out Output buffer's pointer array
 * @param in  Input audio buffer's pointer array
 * @param speed Input speed value pointer array
 */
void wavseq_proc(wavseq_state* st, float* out[], float* in[], float* speed, double* position);
#endif /* _wavseq_H_ */

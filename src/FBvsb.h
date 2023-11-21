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
 * @file FBvsb.h
 * @brief This file implements a variable speed buffer module.
 * @version $LastChangedRevision:$
 * @author Ryo Tanaka
 */

#ifndef _FBVSB_H_
#define _FBVSB_H_

#ifdef __cplusplus
extern "C" {
#endif

/** Internal SBNI state. Should never be accessed directly. */
struct FBVSB_State_;

/** @class FBVSB_State,  FBVSB_State
 * This holds the state of the FBVSB.
 */
typedef struct FBVSB_State_ FBVSB_State;

/** @fn FBVSB_State *FBvsb_init(const char * name, float fs, int frame_size, int nband)
 * @brief This function creates a FBVSB_State component
 * @param name Name of the component
 * @param nch Number of channels
 * @param fs Sampling frequency
 * @param freq_tc Reference frequency of TC signal
 * @return Initialized state of the FBVSB
 */
FBVSB_State *FBvsb_init(const char * name, int nch, int fs, int frame_size, float length_sec);

/** @fn void FBvsb_process(FBVSB_State * restrict st, const float * E2, int nband)
 * @brief This function execute FBvsb in frequency domain
 * @param st    State of the FBVSB
 * @param speed output speed buffer's pointer array
 * @param tcL   input Lch tc signal buffer's pointer array
 * @param tcR   input Rch tc signal buffer's pointer array
 * @param len   Length of the buffer
 */
void FBvsb_process(FBVSB_State * restrict st, float* dst[], float* src[], float* speed);

/** @fn int FBvsb_get_buflen(FBVSB_State * restrict st)
 * @brief This function gets the buffer length
 * @param st State of the FBVSB
 * @return Buffer length in time domain data number
 */
 int FBvsb_get_buflen(FBVSB_State * restrict st);

/** @fn void vsb_set_looplen(VSB_State * restrict st, float ratio)
 * @brief This function sets the loop length
 * @param st State of the FBVSB
 * @param loop_start buffer index of looptop
 * @param loop_len length of loop
 */
void FBvsb_set_loop(FBVSB_State * restrict st, int loop_start, int loop_len);

/** @fn void FBvsb_set_feedbackgain(FBVSB_State * restrict st, float feedbackgain)
 * @brief This function sets the feedbackgain
 * @param st State of the VSB
 * @param feedbackgain feedbackgain
 */
void FBvsb_set_feedbackgain(FBVSB_State * restrict st, float feedbackgain);


#ifdef __cplusplus
} 
#endif

#endif /* _FBVSB_H_ */

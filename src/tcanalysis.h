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
 * @file tcanalysis.h
 * @brief This file implements a module to analyze TC input like tone on Serato TC vinyl
 * @version $LastChangedRevision:$
 * @author Ryo Tanaka
 */

#ifndef _TCANALYSIS_H_
#define _TCANALYSIS_H_

#ifdef __cplusplus
extern "C" {
#endif

/** Internal SBNI state. Should never be accessed directly. */
struct TCANALYSIS_State_;

/** @class TCANALYSIS_State,  TCANALYSIS_State
 * This holds the state of the TCANALYSIS.
 */
typedef struct TCANALYSIS_State_ TCANALYSIS_State;

/** @fn TCANALYSIS_State *tcanalysis_init(const char * name, float fs, int frame_size, int nband)
 * @brief This function creates a TCANALYSIS_State component
 * @param name Name of the component
 * @param fs Sampling frequency
 * @param freq_tc Reference frequency of TC signal
 * @return Initialized state of the TCANALYSIS
 */
TCANALYSIS_State *tcanalysis_init(const char * name, float fs, float freq_tc);

/** @fn void tcanalysis_process(TCANALYSIS_State * restrict st, const float * E2, int nband)
 * @brief This function execute tcanalysis in frequency domain
 * @param st    State of the TCANALYSIS
 * @param speed output speed buffer's pointer array
 * @param tcL   input Lch tc signal buffer's pointer array
 * @param tcR   input Rch tc signal buffer's pointer array
 * @param len   Length of the buffer
 */
void tcanalysis_process(TCANALYSIS_State * restrict st, float* speed, float* tcL, float* tcR, int len);

#ifdef __cplusplus
} 
#endif

#endif /* _TCANALYSIS_H_ */

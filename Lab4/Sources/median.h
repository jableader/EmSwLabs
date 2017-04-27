/*! @file
 *
 *  @brief Median filter.
 *
 *  This contains the functions for performing a median filter on half-word-sized data.
 *
 *  @author PMcL
 *  @date 2015-10-12
 */

#ifndef MEDIAN_H
#define MEDIAN_H

// New types
#include "types.h"

/*! @brief Median filters half-words.
 *
 *  @param array is an array half-words for which the median is sought.
 *  @param size is the length of the array.
 */
int16_t Median_Filter(const int16_t array[], const uint32_t size);

#endif

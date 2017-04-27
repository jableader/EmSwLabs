/*! @file
 *
 *  @brief Median filter.
 *
 *  This contains the functions for performing a median filter on half-word-sized data.
 *
 *  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 *  @date 2016-10-6
 */

#include "median.h"
#include "PE_Types.h"
#include <stdlib.h>
#include <string.h>

#define MEDIAN_ARRAY_SIZE 1024

static int16_t arrayForCloning[MEDIAN_ARRAY_SIZE];

/*! @brief Sort Comparison Function
 *
 * Returns difference between a and b.
 * C's qsort uses function pointers
 *
 * @return int - Negative if a < b, 0 if a == b, Positive if a > b
 */
static int delta(const void * a, const void * b)
{
  const int16_t aInt = *(int16_t *)a, bInt = *(int16_t *)b;

  return (int32_t)aInt - (int32_t)bInt;
}

/*!
 * @addtogroup Median_module Median filter module documentation.
 * @{
 */

int16_t Median_Filter(const int16_t array[], const uint32_t size)
{
  if (size > MEDIAN_ARRAY_SIZE)
    PE_DEBUGHALT();

  // Optimisation: Avoid doing the whole sort for small values.
  switch (size)
  {
  case 0: return 0;
  case 1: return array[0];
  case 2: return (array[0] + array[1]) / 2;
  case 3: return array[1];
  }

  memcpy(arrayForCloning, array, size * sizeof(int16_t));

  // Sort array in place - uses a quick sort variant (efficient) O(n*log(n))
  qsort(arrayForCloning, size, sizeof(int16_t), delta);

  // Calculate the median
  int16_t result;
  if ((size % 2) == 1)
    result = arrayForCloning[size / 2]; // Take middle element.
  else
    // Take average of two middle elements
    result = (arrayForCloning[(size / 2) - 1] + arrayForCloning[size / 2]) / 2;

  return result;
}

/*!
 * @}
 */

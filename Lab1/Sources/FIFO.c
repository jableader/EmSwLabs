/*! @file
 *
 *  @brief Byte Wide FIFO Circular Buffer
 *
 *  This contains the implementation for an array backed byte-wide FIFO Circular Buffer.
 *
 *  Created in Kinetis Design Studio 3.2.0 for the TWR-K70F120M (MK70FN1M0VMJ12 microcontroller)
 *
 *  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 *  @date 2016-08-14
 */

#include "FIFO.h"

void FIFO_Init(TFIFO * const FIFO)
{
  FIFO->NbBytes = 0; // Queue is empty when NbBytes == 0 and full when NbBytes == FIFO_SIZE

  // When the buffer is empty, start index = end index = 0 (front of the buffer)
  FIFO->Start = 0;
  FIFO->End = 0;
}

BOOL FIFO_Put(TFIFO * const FIFO, const uint8_t data)
{
  // Check if buffer full
  if (FIFO->NbBytes == FIFO_SIZE)
  {
    return bFALSE; // FIFO full error: Not able to Put anymore data.
  }

  // Append data to the buffer
  FIFO->Buffer[FIFO->End] = data;
  FIFO->NbBytes++; // Maintain number of bytes in the buffer

  // Increment End index, wrapping to the front if necessary
  FIFO->End = (FIFO->End + 1) % FIFO_SIZE;

  return bTRUE;
}

BOOL FIFO_Get(TFIFO * const FIFO, uint8_t * const dataPtr)
{
  // Check if buffer is empty
  if (FIFO->NbBytes == 0)
  {
    return bFALSE; // FIFO empty error: Nothing to Get.
  }

  // Get data from the buffer
  *dataPtr = FIFO->Buffer[FIFO->Start];
  FIFO->NbBytes--; // Maintain number of bytes in the buffer

  // Increment Start index, wrapping to the front if necessary
  FIFO->Start = (FIFO->Start + 1) % FIFO_SIZE;

  return bTRUE;
}

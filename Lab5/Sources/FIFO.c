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
/*!
 * @addtogroup FIFO_module FIFO module documentation
 * @{
 */
/* MODULE FIFO */

#include "FIFO.h"
#include "Cpu.h"

void FIFO_Init(TFIFO * const FIFO)
{
  FIFO->NbBytes = 0; // Queue is empty when NbBytes == 0 and full when NbBytes == FIFO_SIZE

  // When the buffer is empty, start index = end index = 0 (front of the buffer)
  FIFO->Start = 0;
  FIFO->End = 0;
  FIFO->GetSemaphore = OS_SemaphoreCreate(0);
  FIFO->PutSemaphore = OS_SemaphoreCreate(FIFO_SIZE);
}

bool FIFO_Put(TFIFO * const FIFO, const uint8_t data)
{
  bool result = false;

  // Start critical region to ensure integrity of Buffer
  EnterCritical();

  // Check if buffer full
  if (FIFO->NbBytes < FIFO_SIZE) // FIFO full error: Not able to Put anymore data.
  {
    // Append data to the buffer
    FIFO->Buffer[FIFO->End] = data;
    FIFO->NbBytes++; // Maintain number of bytes in the buffer

    // Increment End index, wrapping to the front if necessary
    FIFO->End = (FIFO->End + 1) % FIFO_SIZE;

    OS_SemaphoreSignal(FIFO->GetSemaphore);
    result = true;
  }

  ExitCritical();

  return result;
}

bool FIFO_Get(TFIFO * const FIFO, uint8_t * const dataPtr)
{
  bool result = false;

  // Start critical region to ensure integrity of Buffer
  EnterCritical();

  // Check if buffer is empty
  if (FIFO->NbBytes > 0) // FIFO empty error: Nothing to Get.
  {
    // Get data from the buffer
    *dataPtr = FIFO->Buffer[FIFO->Start];
    FIFO->NbBytes--; // Maintain number of bytes in the buffer

    // Increment Start index, wrapping to the front if necessary
    FIFO->Start = (FIFO->Start + 1) % FIFO_SIZE;

    OS_SemaphoreSignal(FIFO->PutSemaphore);

    result = true;
  }

  ExitCritical();

  return result;
}

void FIFO_BlockingGet(TFIFO * const FIFO, uint8_t * dataPtr)
{
  if (OS_SemaphoreWait(FIFO->GetSemaphore, 0) != OS_NO_ERROR)
    PE_DEBUGHALT();

  if (!FIFO_Get(FIFO, dataPtr))
    PE_DEBUGHALT();
}

void FIFO_BlockingPut(TFIFO * const FIFO, uint8_t data)
{
  if (OS_SemaphoreWait(FIFO->PutSemaphore, 0) != OS_NO_ERROR)
    PE_DEBUGHALT();

  if (!FIFO_Put(FIFO, data))
    PE_DEBUGHALT();
}

/*!
 * @}
 */

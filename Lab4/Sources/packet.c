/*! @file
 *
 *  @brief 5 Byte Wide Tower to PC Packet Encoder/Decoder
 *
 *  This contains the implementation of Packet encoding/decoding for the Tower Serial Communication Protocol.
 *
 *  Created in Kinetis Design Studio 3.2.0 for the TWR-K70F120M (MK70FN1M0VMJ12 microcontroller)
 *
 *  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 *  @date 2016-08-14
 */
/*!
 * @addtogroup Packet_module Packet module documentation
 * @{
 */
/* MODULE Packet */

#include "packet.h"
#include "UART.h"
#include "Cpu.h"

#define PACKET_SIZE 5

TPacket Packet;

static uint8_t PacketBuf[PACKET_SIZE]; /*!< Internal buffer used with packet error handling */
static uint8_t BufNbBytes; /*!< Internal index for packet error handling */

bool Packet_Init(const uint32_t baudRate, const uint32_t moduleClk)
{
  // Initialise the UART and receive/transmit buffers
  (void) UART_Init(baudRate, moduleClk);

  // Initialise the internal buffer's index
  BufNbBytes = 0;
  return true;
}

/*! @brief Read a byte from the UART receive buffer
 *
 *  Appends a byte to the internal packet buffer if there is
 *  packet available in the UART Receive Buffer.
 *
 *  @return BOOL - TRUE if a packet was read into the internal buffer.
 */
static bool ReadNextByte(void)
{
  uint8_t data;

  // Check if there is a byte in the receive buffer
  if (UART_InChar(&data))
  {
    // Append the internal buffer with the received byte
    PacketBuf[BufNbBytes] = data;
    BufNbBytes++;
    return true;
  }

  // No bytes in the receive buffer
  return false;
}

/*! @brief Checks if the candidate packet in the buffer is valid
 *
 *  Verifies the candidate packet in the buffer by comparing the checksum byte
 *  against one calculated for the initial 4 bytes of the packet
 *
 *  @return BOOL - TRUE if the candidate packet is successful.
 */
static bool IsChecksumValid(void)
{
  uint8_t checksum = PacketBuf[0]; // set Initial Value (byte 1) for checksum calculation
  uint8_t i;

  // Perform checksum calculation
  for (i = 1; i < (PACKET_SIZE - 1); i++)
  {
    checksum ^= PacketBuf[i];
  }

  // Check if calculated checksum == received checksum
  return checksum == PacketBuf[PACKET_SIZE - 1];
}

/*! @brief Discard First Byte In Internal Buffer
*
* Drops the first byte in the internal error handling/recovery buffer and maintains buffer state
*
*/
static void ShiftBuffer(void)
{
  uint8_t i;

  // Shift the internal buffer 1 to the left (discard first byte)
  for (i = 1; i < PACKET_SIZE; i++)
  {
    PacketBuf[i - 1] = PacketBuf[i];
  }

  BufNbBytes--;
}

/*! @brief Set Packet Bytes and Reset Internal Buffers
*
* Sets the public Packet bytes and resets the internal state used for error handling/recovery
*
*/
static void SetValuesAndResetBuffer(void)
{
  // Set the packet bytes from the buffer used during the error checking/handling phase
  Packet_Command = PacketBuf[0];
  Packet_Parameter1 = PacketBuf[1];
  Packet_Parameter2 = PacketBuf[2];
  Packet_Parameter3 = PacketBuf[3];

  // Reset the error handling buffer index
  BufNbBytes = 0;
}

bool Packet_Get(void)
{
  // Continuously receive bytes until a valid packet is formed or the receive buffer is empty
  while (ReadNextByte())
  {
    // Check if the internal buffer has enough bytes for a packet
    if (BufNbBytes >= PACKET_SIZE)
    {
      // Check if the candidate (formed) packet is valid
      if (IsChecksumValid())
      {
        // Set the Packet bytes and reset internal error handling/recovery state
        SetValuesAndResetBuffer();
        return true;
      }
      else
      {
        // Candidate packet was invalid
        // Attempt error recovery by discarding first byte (in preparation for reading another byte)
        ShiftBuffer();
      }
    }
  }

  // No valid packet could be formed
  return false;
}

bool Packet_Put(const uint8_t command, const uint8_t parameter1,
    const uint8_t parameter2, const uint8_t parameter3)
{
  // Calculate check sum
  uint8_t checkSum = command ^ parameter1 ^ parameter2 ^ parameter3;

  // We could be interrupted by RTC, which may push a packet
  // through half way through transmitting this one
  EnterCritical();

  // Transmit the packet bytes
  const bool wasSuccess = UART_OutChar(command) &&
      UART_OutChar(parameter1) &&
      UART_OutChar(parameter2) &&
      UART_OutChar(parameter3) &&
      UART_OutChar(checkSum);

  ExitCritical();

  return wasSuccess;
}

/*!
 * @}
 */

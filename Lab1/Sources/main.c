/*! @file
 *
 *  @brief Tower Serial Communications (Lab1)
 *
 *  This contains the implementation of the UART peripheral on the K70
 *  as well as the Tower Serial Communication Protocol.
 *
 *  Created in Kinetis Design Studio 3.2.0 for the TWR-K70F120M (MK70FN1M0VMJ12 microcontroller)
 *
 *  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 *  @date 2016-08-14
 */

#include "Cpu.h"
#include "types.h"
#include "UART.h"
#include "Packet.h"

const uint8_t PACKET_ACK_MASK = 1 << 7; // Command ID has bit 7 (MSB) reserved for packet acknowledgement
const uint32_t BAUD_RATE = 38400; // Either 38400 or 115200 baud. Default is 38400.

// Enum for Tower Command Packet opcodes
enum TowerCommand
{
  STARTUP = 0x04, // "Tower Startup" / "Get startup values" Command
  SPECIAL = 0x09, // "Special - Tower version" / "Special -  Get startup values" Command
  TOWER_NUMBER = 0x0B // "Tower Number" Command
};

static uint16union_t TowerNumber; /*!< The Tower's Number */

/*! @brief Send the "Tower Startup" packet
*
* Command: 0x04
* Parameter 1: 0
* Parameter 2: 0
* Parameter 3: 0
*
* @note The Tower will issue this command upon startup to allow the PC to update the interface application
* @note and the Tower. Typically, setup data will also be sent from the Tower to the PC.
*
*/
static void SendStartup(void)
{
  (void) Packet_Put(STARTUP, 0, 0, 0);
}

/*! @brief Send the "Tower version" response packet
*
* For now, the “0x09 Special –Tower Version” should be V1.0
*
* Command: 0x09
* Parameter 1: ‘v’ = version
* Parameter 2: Major Version Number
* Parameter 3: Minor Version Number (out of 100)
* @note e.g. V1.3 has a major version number of 1 and a minor version number of 30.
*
*/
static void SendVersion(void)
{
  (void) Packet_Put(SPECIAL, 'v', 1, 0);
}

/*! @brief Send the "Tower number" response packet
*
* CommandL 0x0B
* Parameter 1: 1
* Parameter 2: LSB
* Parameter 3: MSB
*
*/
static void SendTowerNumber(void)
{
  (void) Packet_Put(TOWER_NUMBER, 1, TowerNumber.s.Lo, TowerNumber.s.Hi);
}

/*! @brief Handles the "Get startup values" packet
*
* Command: 0x04
* Parameter 1: 0
* Parameter 2: 0
* Parameter 3: 0
*
* In response to the "Get startup values" packet, we should transmit the following packets:
* - a “0x04 Tower startup” packet
* - a “0x09 Special – Tower version” packet
* - a “0x0B Tower number” packet
*
*  @return BOOL - TRUE if the packet was successfully handled.
*/
static BOOL HandleStartup(void)
{
  if (Packet_Parameter1 == 0
      && Packet_Parameter2 == 0
      && Packet_Parameter3 == 0)
  {
    // Transmit the three required packets to the PC
    SendStartup();
    SendVersion();
    SendTowerNumber();
    return bTRUE;
  }

  return bFALSE;
}

/*! @brief Handles the Special Command (currently only Get version implemented)
*
* Sends the version number to the PC.
*
* Command: 0x09
* Parameter 1: ‘v’
* Parameter 2: ‘x’
* Parameter 3: CR
*
*  @return BOOL - TRUE if the packet was successfully handled.
*/
static BOOL HandleSpecial(void)
{
  // Verify that the received command was for "Get version"
  if (Packet_Parameter1 == 'v'
      && Packet_Parameter2 == 'x'
      && Packet_Parameter3 == '\r') // CR
  {
    // Transmit the version number to the PC
    SendVersion();
    return bTRUE;
  }

  // Invalid command, likely unimplemented "special" command
  return bFALSE;
}

/*! @brief Handles the Tower Number PC To Tower Command
*
* Command: 0x0B
* Parameter 1: 1 = get Tower number
*              2 = set Tower number
* Parameter 2: LSB for a “set”, 0 for a “get”
* Parameter 3: MSB for a “set”, 0 for a “get”
* @note The Tower number is an unsigned 16-bit number
*
*  @return BOOL - TRUE if the packet was successfully handled.
*/
static BOOL HandleTowerNumber(void)
{
  if (Packet_Parameter1 == 1 // 1 = get Tower Number
      // Verify Parameter 2 and 3
      && Packet_Parameter2 == 0 // 0 for a "get"
      && Packet_Parameter3 == 0) // 0 for a "get"
  {
    // Transmit the Tower Number to the PC
    SendTowerNumber();
    return bTRUE;
  }
  else if (Packet_Parameter1 == 2) // 2 = set Tower Number
  {
    // Set the TowerNumber
    TowerNumber.s.Lo = Packet_Parameter2; // Parameter 2 for LSB
    TowerNumber.s.Hi = Packet_Parameter3; // Parameter 3 for MSB
    return bTRUE;
  }

  // Invalid packet, likely called get with non zeroed parameter 2/3
  return bFALSE;
}

/*! @brief Handles the received and verified packet based on its command byte.
*
*  @return BOOL - TRUE if the packet was successfully handled.
*/
static BOOL HandlePacket(void)
{
  // Switch on Packet Command after zeroing acknowledgment bit
  switch (Packet_Command & ~PACKET_ACK_MASK)
  {
  // Respond to a Startup Packet (with packet param checking)
  case STARTUP:
    return HandleStartup();

  // Respond to a Special Packet (currently used only for Get Version)
  case SPECIAL:
    return HandleSpecial();

  // Respond to a Get/Set Tower Number Packer
  case TOWER_NUMBER:
    return HandleTowerNumber();

  // Received invalid or unimplemented packet
  default:
    return bFALSE;
  }
}

/*! @brief Sends the ACK/NAK packet if the Packet Command requires it
*
*   If acknowledgement is requested, the received packet is echoed.
*   Bit 7 in the Command byte is used to indicate that the command was successful (ACK/NAK)
*
*  @param wasSuccess Whether the packet was handled successfully
*/
static void SendAcknowledgeIfRequired(BOOL wasSuccess)
{
  // Check if Acknowledgement was requested
  if (Packet_Command & PACKET_ACK_MASK)
  {
    // Create mask for ACK/NAK
    // Makes: X111 1111, where X is wasSuccess
    const uint8_t acknowledgeMask = (wasSuccess << 7) | ~PACKET_ACK_MASK;

    // Echo the Packet with bit acknowledgement flag set on the command byte
    (void) Packet_Put(Packet_Command & acknowledgeMask, Packet_Parameter1, Packet_Parameter2, Packet_Parameter3);
  }
}

/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/

  // Initialise the Tower number to the last 4 digits of Jacob's student number
  TowerNumber.l = 4718;

  // Initialise the Packet Encoder/Decoder (+ UART) to the BAUD_RATE specified above and the bus clock speed from Processor Expert
  (void) Packet_Init(BAUD_RATE, CPU_BUS_CLK_HZ); // No error handling required, cannot currently fail

  // Send startup packets as per Tower To PC Protocol
  HandleStartup();

  // Loop forever (embedded software never ends!)
  for (;;)
  {
    // Poll the UART: Receive/Transmit packets into/from the FIFO buffers as required
    UART_Poll();

    // Check if a valid packet can be built from the Receive Buffer
    if (Packet_Get())
    {
      // Handle the received Packet based on the Packet Command
      const BOOL correctlyHandled = HandlePacket();

      // Transmit ACK/NAK packet to the PC if required
      SendAcknowledgeIfRequired(correctlyHandled);
    }
  }

  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
#ifdef PEX_RTOS_START
  PEX_RTOS_START(); /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
#endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for (;;)
  {
  }
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

/* END main */
/*!
 ** @}
 */
/*
 ** ###################################################################
 **
 **     This file was created by Processor Expert 10.5 [05.21]
 **     for the Freescale Kinetis series of microcontrollers.
 **
 ** ###################################################################
 */

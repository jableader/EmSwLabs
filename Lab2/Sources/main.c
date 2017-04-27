/*! @file
 *
 *  @brief Tower MCG and Flash Memory (Lab2)
 *
 *  This contains the implementation of the Tower MCG and Flash Memory on the K70
 *  as well as the implementation of prior lab functionality as per the specification.
 *
 *  Created in Kinetis Design Studio 3.2.0 for the TWR-K70F120M (MK70FN1M0VMJ12 microcontroller)
 *
 *  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 *  @date 2016-08-29
 */

// CPU module - contains low level hardware initialization routines
#include "Cpu.h"
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"
#include "types.h"
#include "UART.h"
#include "Packet.h"
#include "LEDs.h"
#include "Flash.h"

const uint8_t PACKET_ACK_MASK = 1 << 7; // Command ID has bit 7 (MSB) reserved for packet acknowledgement
const uint32_t BAUD_RATE = 115200; // Either 38400 or 115200 baud. Default is 38400.

// Enum for Tower Command Packet opcodes
enum TowerCommand
{
  STARTUP = 0x04, // "Tower Startup" / "Get startup values" Command
  FLASH_PROG = 0x07, // "Flash - Program Byte" Command
  FLASH_READ = 0x08, // "Flash - Read Byte" Command
  SPECIAL = 0x09, // "Special - Tower version" / "Special -  Get startup values" Command
  TOWER_NUMBER = 0x0B, // "Tower Number" Command
  TOWER_MODE = 0x0D, // "Tower Mode" Command
};

static volatile uint16union_t * NvTowerNb; /*! The Tower's Number */
static volatile uint16union_t * NvTowerMode; /*! The Tower's Mode */

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
  (void) Packet_Put(TOWER_NUMBER, 1, NvTowerNb->s.Lo, NvTowerNb->s.Hi);
}

/*! @brief Send the "Tower Mode" response packet
 *
 * Command: 0x0D
 * Parameter 1: 1
 * Parameter 2: LSB
 * Parameter 3: MSB
 *
 */
static void SendTowerMode(void)
{
  (void) Packet_Put(TOWER_MODE, 1, NvTowerMode->s.Lo, NvTowerMode->s.Hi);
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
 * - a “0x0B Tower Number” packet
 * - a "0x0D Tower Mode" packet
 *
 *  @return bool - TRUE if the packet was successfully handled.
 */
static bool HandleStartup(void)
{
  if (Packet_Parameter1 == 0
      && Packet_Parameter2 == 0
      && Packet_Parameter3 == 0)
  {
    // Transmit the four required packets to the PC
    SendStartup();
    SendVersion();
    SendTowerNumber();
    SendTowerMode();
    return true;
  }

  return false;
}

/*! @brief Handles the "Flash - Program byte" packet
 *
 * Command: 0x07
 * Parameter 1: When 0-7 Address offset, when 8 'erase sector'
 * Parameter 2: 0
 * Parameter 3: data
 *
 * No response
 *
 * @return bool - TRUE if the packet was successfully handled.
 */
static bool HandleProgramByte(void)
{
  if (Packet_Parameter2 != 0 || Packet_Parameter1 > 8)
    return false;

  if (Packet_Parameter1 == 8)
    return Flash_Erase();

  volatile uint8_t * address = (uint8_t *) (FLASH_DATA_START + Packet_Parameter1);
  return Flash_Write8(address, Packet_Parameter3);
}

/*! @brief Handles the "Flash - Read Byte" packet
 *
 * Command: 0x08,
 * Parameter 1: Offset (0-7)
 * Parameter 2: 0
 * Parameter 3: 0
 *
 * Response: Send the "Flash Byte" packet
 *
 * @return bool - TRUE if the packet was successfully handled.
 */
static bool HandleReadByte(void)
{
  if (Packet_Parameter23 != 0 || Packet_Parameter1 > 7)
    return false;

  // Keep consistent with other impl that dont return false when the packet put fails
  (void) Packet_Put(FLASH_READ, Packet_Parameter1, 0, _FB(FLASH_DATA_START + Packet_Parameter1));
  return true;
}

/*! @brief Handles the "Tower Number" packet
 *
 * Command: 0x0D
 * Parameter 1:  1 = get Tower mode
 *               2 = set Tower mode
 * Parameter 2: LSB for a “set”, 0 for a “get”
 * Parameter 3: MSB for a “set”, 0 for a “get”
 *
 * Response: None
 *
 * @return bool - TRUE if the packet was successfully handled.
 */
static bool HandleTowerMode(void)
{
  if (Packet_Parameter1 == 1 && Packet_Parameter23 == 0)
  {
    SendTowerMode();
    return true;
  }
  else if (Packet_Parameter1 == 2)
  {
    return Flash_Write16((uint16_t *)NvTowerMode, Packet_Parameter23);
  }

  return false;
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
 *  @return bool - TRUE if the packet was successfully handled.
 */
static bool HandleSpecial(void)
{
  // Verify that the received command was for "Get version"
  if (Packet_Parameter1 == 'v' && Packet_Parameter2 == 'x'
      && Packet_Parameter3 == '\r') // CR
  {
    // Transmit the version number to the PC
    SendVersion();
    return true;
  }

  // Invalid command, likely unimplemented "special" command
  return false;
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
 *  @return bool - TRUE if the packet was successfully handled.
 */
static bool HandleTowerNumber(void)
{
  if (Packet_Parameter1 == 1 // 1 = get Tower Number
  // Verify Parameter 2 and 3
  && Packet_Parameter23 == 0) // 0 for a "get"
  {
    // Transmit the Tower Number to the PC
    SendTowerNumber();
    return true;
  }
  else if (Packet_Parameter1 == 2) // 2 = set Tower Number
  {
    return Flash_Write16((uint16_t *) NvTowerNb, Packet_Parameter23);
  }

  // Invalid packet, likely called get with non zeroed parameter 2/3
  return false;
}

/*! @brief Handles the received and verified packet based on its command byte.
 *
 *  @return bool - TRUE if the packet was successfully handled.
 */
static bool HandlePacket(void)
{
  // Switch on Packet Command after zeroing acknowledgment bit
  switch (Packet_Command & ~PACKET_ACK_MASK)
  {
  // Respond to a Startup Packet (with packet param checking)
  case STARTUP:
    return HandleStartup();

  case FLASH_PROG:
    return HandleProgramByte();

  case FLASH_READ:
    return HandleReadByte();

    // Respond to a Special Packet (currently used only for Get Version)
  case SPECIAL:
    return HandleSpecial();

    // Respond to a Get/Set Tower Number Packer
  case TOWER_NUMBER:
    return HandleTowerNumber();

  case TOWER_MODE:
    return HandleTowerMode();

    // Received invalid or unimplemented packet
  default:
    return false;
  }
}

/*! @brief Sends the ACK/NAK packet if the Packet Command requires it
 *
 *   If acknowledgement is requested, the received packet is echoed.
 *   Bit 7 in the Command byte is used to indicate that the command was successful (ACK/NAK)
 *
 *  @param wasSuccess Whether the packet was handled successfully
 */
static void SendAcknowledgeIfRequired(bool wasSuccess)
{
  // Check if Acknowledgement was requested
  if (Packet_Command & PACKET_ACK_MASK)
  {
    // Create mask for ACK/NAK
    // Makes: X111 1111, where X is wasSuccess
    const uint8_t acknowledgeMask = (wasSuccess << 7) | ~PACKET_ACK_MASK;

    // Echo the Packet with bit acknowledgement flag set on the command byte
    (void) Packet_Put(Packet_Command & acknowledgeMask, Packet_Parameter1,
    Packet_Parameter2, Packet_Parameter3);
  }
}

/*! @brief Initialises the Packet, Flash and LEDs modules.
 *
 * @return bool - true if all modules were successfully initialised
 */
static bool InitializeComponents(void)
{
  return Packet_Init(BAUD_RATE, CPU_BUS_CLK_HZ) & Flash_Init() & LEDs_Init();
}

/*! @brief Allocates a block of Flash Memory and sets a default if the block is empty
 *
 *  Maps an addressPtr to a block of FLASH memory of the given size.
 *  If the block of memory is empty a default is set.
 *
 *  @param addressPtr Pointer to the address to be mapped to memory
 *  @param dataIfEmpty The data that should be set to the block if empty
 */
static void AllocateAndSet(volatile uint16union_t ** const addressPtr, uint16_t const dataIfEmpty)
{
  // Allocate block in memory for the passed in size
  bool allocatedAddress = Flash_AllocateVar((volatile void **)addressPtr, sizeof(**addressPtr));

  if (allocatedAddress && (*addressPtr)->l == 0xFFFF)
  {
    // Memory "empty", set default
    Flash_Write16((uint16 *)*addressPtr, dataIfEmpty);
  }
}

/*! @brief The main entry point into the program
 *
 *  @return int - Hopefully never (embedded software never ends!)
 */
/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/

  // Initialse the components on the Tower Board (UART, Flash, LEDs etc.)
  if (InitializeComponents())
  {
    // If Tower Board is successful in starting up (all peripherals initialised), then the orange LED should be turned on.
    LEDs_On(LED_ORANGE);
  }

  // Allocate flash memory for Tower Mode and Number, and set defaults if empty
  AllocateAndSet(&NvTowerMode, 1); // default to 1 as per spec
  AllocateAndSet(&NvTowerNb, 4718); // default to last 4 digits of student number (Jacob's) as per spec

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
      const bool correctlyHandled = HandlePacket();

      // Transmit ACK/NAK packet to the PC if required
      SendAcknowledgeIfRequired(correctlyHandled);
    }
  }

  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
  #ifdef PEX_RTOS_START
    PEX_RTOS_START();                  /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;){}
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

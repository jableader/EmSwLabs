/*!
 ** @file main.c
 ** @version 4.0
 ** @brief RTOS (Lab 5)
 **
 **  This contains the implementation of the Tower MCG and Flash Memory on the K70
 **  as well as the implementation of prior lab functionality as per the specification.
 **
 **  Created in Kinetis Design Studio 3.2.0 for the TWR-K70F120M (MK70FN1M0VMJ12 microcontroller)
 **
 **  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 **  @date 2016-10-31
 **/
/*!
 **  @addtogroup main_module main module documentation
 **  @{
 */
/* MODULE main */

// CPU module - contains low level hardware initialization routines
#include "Cpu.h"
#include "Events.h"
#include "INT_UART2_RX_TX.h"
#include "INT_RTC_Seconds.h"
#include "INT_PIT0.h"
#include "INT_FTM0.h"
#include "INT_PendableSrvReq.h"
#include "INT_SysTick.h"
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"
#include "types.h"
#include "Packet.h"
#include "LEDs.h"
#include "Flash.h"
#include "RTC.h"
#include "PIT.h"
#include "FTM.h"
#include "analog.h"
#include "median.h"
#include "OS.h"

#define THREAD_STACK_SIZE 200

// Commenting the below out disables analog packets in async mode
//#define TRANSMIT_ASYNC_PACKETS

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
  TIME = 0x0C, // "Time" Command
  TOWER_MODE = 0x0D, // "Tower Mode" Command
  PROTOCOL_MODE = 0x0A, // "Protocol - Mode" Command
  ANALOG_INPUT = 0x50, // "Analog Input - Value" Command
};

// Enum for Tower Protocol Mode
typedef enum
{
  ASYNCHRONOUS = 0, /*! Asynchronous protocol mode */
  SYNCHRONOUS = 1, /*! Synchronous protocol mode */
} ProtocolMode;

// Struct for the analog processing thead
typedef struct
{
  uint8_t ChannelNb; /*! Channel Number */
  OS_ECB* Semaphore; /*! Semaphore for the analog processing thread */
  TAnalogInput* Values;  /*! Analog values */
} TAnalogThread;

static volatile uint16union_t * NvTowerNb; /*! The Tower's Number */
static volatile uint16union_t * NvTowerMode; /*! The Tower's Mode */
static ProtocolMode TowerProtocolMode; /* The Tower's Protocol Mode */

static uint32_t ProtocolProcessingThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08))); /*! The stack for the protocol responses. */
static uint32_t RTCThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08))); /*! The stack for the RTC thread. */
static uint32_t FTMThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08))); /*! The stack for the FTM thread. */

static TAnalogThread AnalogProcessingThreadSettings[ANALOG_NB_INPUTS]; /*! The settings for the Analog Processing threads */
static uint32_t AnalogProcessingThreadStack[THREAD_STACK_SIZE * ANALOG_NB_INPUTS] __attribute__ ((aligned(0x08))); /*! The stack for the processing of analog data. */

static TFTMChannel LedTimerChannel; /*! The timer channel + settings for the FTM */

static OS_ECB* RTCSemaphore; /*! The semaphore for the RTC to signal */

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
 * For now, the '0x09 Special 'Tower Version' should be V1.0
 *
 * Command: 0x09
 * Parameter 1: 'v' = version
 * Parameter 2: Major Version Number
 * Parameter 3: Minor Version Number (out of 100)
 * @note e.g. V1.3 has a major version number of 1 and a minor version number of 30.
 *
 */
static void SendVersion(void)
{
  (void) Packet_Put(SPECIAL, 'v', 5, 0);
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

/*! @brief Send the "Protocol - Mode" response packet
 *
 * Command: 0x0A
 * Parameter 1: 1
 * Parameter 2: 0 = asynchronous
 *              1 = synchronous
 * Parameter 3: 0
 *
 */
static void SendProtocolMode(void)
{
  (void) Packet_Put(PROTOCOL_MODE, 1, TowerProtocolMode, 0);
}

/*! @brief Send the "Time" packet
 *
 * Command: 0x0C
 * Parameter 1: hours (0-23)
 * Parameter 2: minutes (0-59)
 * Parameter 3: seconds (0-59)
 *
 */
static void SendTime(void)
{
  // Get value from the real time clock
  uint8_t hours, minutes, seconds;
  RTC_Get(&hours, &minutes, &seconds);

  // Transmit time
  (void) Packet_Put(TIME, hours, minutes, seconds);
}

/*! @brief Send the "Analog Input - Value" packet
 *
 * Command: 0x50
 * Parameter 1: Channel Nb (0-7) (currently only 0 & 1 are implemented)
 * Parameter 2: LSB
 * Parameter 3: MSB
 */
static void SendAnalogValue(uint8_t channelNb, int16union_t value)
{
  (void) Packet_Put(ANALOG_INPUT, channelNb, value.s.Lo, value.s.Hi);
}

/*! @brief Handles the "Get startup values" packet
 *
 * Command: 0x04
 * Parameter 1: 0
 * Parameter 2: 0
 * Parameter 3: 0
 *
 * In response to the "Get startup values" packet, we should transmit the following packets:
 * - a '0x04 Tower startup' packet
 * - a '0x09 Special ' Tower version' packet
 * - a '0x0B Tower Number' packet
 * - a "0x0D Tower Mode" packet
 * - a "0x0A Protocol - Mode" packet
 *
 *  @return bool - TRUE if the packet was successfully handled.
 */
static bool HandleStartup(void)
{
  if (Packet_Parameter1 == 0 && Packet_Parameter2 == 0
      && Packet_Parameter3 == 0)
  {
    // Transmit the five required packets to the PC
    SendStartup();
    SendVersion();
    SendTowerNumber();
    SendTowerMode();
    SendProtocolMode();
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
  // Validate parameters
  if (Packet_Parameter2 != 0 || Packet_Parameter1 > 8)
    return false;

  if (Packet_Parameter1 == 8)
    return Flash_Erase();

  // Write to Flash
  volatile uint8_t * address =
      (uint8_t *) (FLASH_DATA_START + Packet_Parameter1);
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
  (void) Packet_Put(FLASH_READ, Packet_Parameter1, 0,
      _FB(FLASH_DATA_START + Packet_Parameter1));
  return true;
}

/*! @brief Handles the "Tower Number" packet
 *
 * Command: 0x0D
 * Parameter 1:  1 = get Tower mode
 *               2 = set Tower mode
 * Parameter 2: LSB for a 'set', 0 for a 'get'
 * Parameter 3: MSB for a 'set', 0 for a 'get'
 *
 * Response: None
 *
 * @return bool - TRUE if the packet was successfully handled.
 */
static bool HandleTowerMode(void)
{
  if (Packet_Parameter1 == 1 && Packet_Parameter23 == 0)
  {
    // Get tower mode
    SendTowerMode();
    return true;
  }
  else if (Packet_Parameter1 == 2)
  {
    // Set tower mode
    return Flash_Write16((uint16_t *) NvTowerMode, Packet_Parameter23);
  }

  // Invalid command
  return false;
}

/*! @brief Handles the "Protocol - Mode" packet
 *
 * Command: 0x0A
 * Parameter 1:  1 = get Protocol mode
 *               2 = set Protocol mode
 * Parameter 2: 0 = asynchronous for a 'set', 0 for a 'get'
 *              1 = synchronous for a 'set', 0 for a 'get'
 * Parameter 3: 0
 *
 * Response: None
 *
 * @return bool - TRUE if the packet was successfully handled.
 */
static bool HandleProtocolMode(void)
{
  if (Packet_Parameter1 == 1 && Packet_Parameter23 == 0)
  {
    // Get protocol mode
    SendProtocolMode();
    return true;
  }
  else if (Packet_Parameter1 == 2 && Packet_Parameter2 <= 1 && Packet_Parameter3 == 0)
  {
    // Set Protocol Mode
    TowerProtocolMode = Packet_Parameter2;
    return true;
  }

  // Invalid command
  return false;
}

/*! @brief Handles the Special Command (currently only Get version implemented)
 *
 * Sends the version number to the PC.
 *
 * Command: 0x09
 * Parameter 1: 'v'
 * Parameter 2: 'x'
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
 * Parameter 2: LSB for a 'set', 0 for a 'get'
 * Parameter 3: MSB for a 'set', 0 for a 'get'
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

/*! @brief Handles the Set Time PC To Tower Command
 *
 * Command: 0x0C
 * Parameter 1: hours (0-23)
 * Parameter 2: minutes (0-59)
 * Parameter 3: seconds (0-59)
 *
 *  @return bool - TRUE if the packet was successfully handled.
 */
static bool HandleSetTime(void)
{
  // Check that the parameters are a valid time
  if ((Packet_Parameter1 >= 24) || (Packet_Parameter2 >= 60)
      || (Packet_Parameter3 >= 60))
    return false;

  // Set the RTC clock
  RTC_Set(Packet_Parameter1, Packet_Parameter2, Packet_Parameter3);
  return true;
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
  case STARTUP:
    return HandleStartup();

  case FLASH_PROG:
    return HandleProgramByte();

  case FLASH_READ:
    return HandleReadByte();

  case SPECIAL:
    return HandleSpecial();

  case TOWER_NUMBER:
    return HandleTowerNumber();

  case TIME:
    return HandleSetTime();

  case TOWER_MODE:
    return HandleTowerMode();

  case PROTOCOL_MODE:
    return HandleProtocolMode();

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

/*! @brief Callback function used when servicing the PIT Interrupt.
 *   Expected to be called every 10ms.
 *
 *   Toggles the Green LED every 50 calls (500 ms).
 *
 *   Takes analog samples from the ADC every 10ms, and sends an 'analog input'
 *   packet when needed as per spec.
 *
 *  @param void* args Not used, arguments which may be used in future - for complying with callback interface.
 */
static void PITCallback(void* args)
{
  // Sample analog channels
  for (uint8_t i = 0; i < ANALOG_NB_INPUTS; i++)
  {
    if (Analog_Get(i))
    {
      // If we receive an analog value, signal the processing background thread
      OS_SemaphoreSignal(AnalogProcessingThreadSettings[i].Semaphore);
    }
  }

  // Toggle LED is sharing this callback with the analog samples.
  // Since we're sampling at 10ms and wish to toggle every 500ms we can simply
  // count to 50.
  static uint8_t toggleCallback = 0;
  if (++toggleCallback == 50)
  {
    LEDs_Toggle(LED_GREEN);
    toggleCallback = 0;
  }
}

/*! @brief Initialises the Packet, Flash, LED, RTC, PIT, OS, FTM and Analog modules.
 *  Switches on the Orange LED when successful
 *
 * @return bool - true if all modules were successfully initialised
 * @note Will halt on failure
 */
static void InitializeComponents(void)
{
  // If Tower Board is successful in starting up (all peripherals initialised), then the orange LED should be turned on.
  OS_Init(CPU_BUS_CLK_HZ, false);

  RTCSemaphore = OS_SemaphoreCreate(0);

  bool worked = Packet_Init(BAUD_RATE, CPU_BUS_CLK_HZ) & Flash_Init()
      & LEDs_Init() & RTC_Init(RTCSemaphore)
      & PIT_Init(CPU_BUS_CLK_HZ, &PITCallback, NULL) & FTM_Init()
      & Analog_Init(CPU_BUS_CLK_HZ);

  if (worked)
    LEDs_On(LED_ORANGE);
  else
    PE_DEBUGHALT();
}

/*! @brief Allocates a block of Flash Memory and sets a default if the block is empty
 *
 *  Maps an addressPtr to a block of FLASH memory of the given size.
 *  If the block of memory is empty a default is set.
 *
 *  @param addressPtr Pointer to the address to be mapped to memory
 *  @param dataIfEmpty The data that should be set to the block if empty
 */
static void AllocateAndSet(volatile uint16union_t ** const addressPtr,
    uint16_t const dataIfEmpty)
{
  // Allocate block in memory for the passed in size
  bool allocatedAddress = Flash_AllocateVar((volatile void **) addressPtr,
      sizeof(**addressPtr));

  if (allocatedAddress && (*addressPtr)->l == 0xFFFF)
  {
    // Memory "empty", set default
    Flash_Write16((uint16 *) *addressPtr, dataIfEmpty);
  }
}


/*! @brief Wait indefinitely on the provided semaphore, with debug halt on failure
 *
 *  @param semaphore The semaphore to wait on
 */
static void WaitForever(OS_ECB * semaphore)
{
  // Wait on the semaphore to be signalled indefinitely
  OS_ERROR value = OS_SemaphoreWait(semaphore, 0);

  // Halt if there is an unexpected error
  if (value != OS_NO_ERROR)
  {
    PE_DEBUGHALT();
  }
}

/*! @brief Toggles the Yellow LED and transmits a 0x0C time packet to the PC when the RTCSemaphore is signalled
 *
 *  @param void* args Not used, arguments which may be used in future - for complying with callback interface.
 */
static void RTCTimerThread(void* args)
{
  for (;;)
  {
    // Wait for RTC to be signalled
    WaitForever(RTCSemaphore);

    LEDs_Toggle(LED_YELLOW); // Toggle the Yellow LED
    SendTime(); // Transmit a time packet to the PC
  }
}

/*! @brief Turns off the Blue LED (which is turned on externally) when the ledTimerChannel semaphore is signalled
 *
 *  @param void* args Not used, arguments which may be used in future - for complying with callback interface.
 */
static void FTMLightThread(void* args)
{
  for (;;)
  {
    WaitForever(LedTimerChannel.semaphore); // Wait for the time out

    LEDs_Off(LED_BLUE); // Turn off the Blue LED (turned on externally)
  }
}

/*! @brief Thread that processes and transmits the analog data recieved from the
 *  DAC.
 *  @param args A pointer to a TAnalogThread struct containing the configuration for this thread
 */
static void AnalogProcessingThread(void * arg)
{
  TAnalogThread settings = *(TAnalogThread*) arg;
  for (;;)
  {
    // Wait for the analog thread to be signalled (i.e. sample a value)
    WaitForever(settings.Semaphore);

    settings.Values->oldValue = settings.Values->value; // Set old value

    // Set value to the current median of "sliding window"
    settings.Values->value.l = Median_Filter(settings.Values->values, ANALOG_WINDOW_SIZE);

    // From the spec:
    // When SYNCHRONOUS: Send every 10ms
    // When ASYNCHRONOUS: Send when value has changed, at intervals no greater than 10ms
    if (TowerProtocolMode == SYNCHRONOUS
#ifdef TRANSMIT_ASYNC_PACKETS
        || (settings.Values->oldValue.l != settings.Values->value.l)
#endif
        )

    {
      // Transmit analog value to the PC
      SendAnalogValue(settings.ChannelNb, settings.Values->value);
    }
  }
}

/*! @brief Thread that listens for and responds to packets for the Tower Protocol.
 *
 *  @param ignored Unused
 */
static void ProtocolProcessingThread(void * ignored)
{
  // Send startup packets as per Tower To PC Protocol
  HandleStartup();

  for (;;)
  {
    // Blocking call, awaits valid packet
    Packet_Get();

    // "On reception of a valid packet from the PC, the blue LED must be turned
    // on for a period of one  second"
    //
    // ASSUME: Valid packet means 4 bytes with a matching checksum,
    // not that the parameters are valid & handled (i.e. what the ACK means)
    LEDs_On(LED_BLUE);
    FTM_StartTimer(&LedTimerChannel); // (Asynchronously) turn off the Blue LED after 1 second

    // Handle the received Packet based on the Packet Command
    const bool correctlyHandled = HandlePacket();

    // Transmit ACK/NAK packet to the PC if required
    SendAcknowledgeIfRequired(correctlyHandled);
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
  __DI()
  ;
  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/

  // Initialse the components on the Tower Board (UART, Flash, LEDs etc.)
  InitializeComponents();

  // Allocate flash memory for Tower Mode and Number, and set defaults if empty
  AllocateAndSet(&NvTowerMode, 1); // default to 1 as per spec
  AllocateAndSet(&NvTowerNb, 4718); // default to last 4 digits of student number (Jacob's) as per spec

  // Set up the FTM interrupt to call a function after 1 second
  // Used to turn off the Blue LED after a second after turning it on
  // after receiving a valid packet
  LedTimerChannel.channelNb = 0; // Use channel 0
  LedTimerChannel.delayCount = CPU_MCGFF_CLK_HZ_CONFIG_0; // 1 second
  LedTimerChannel.timerFunction = TIMER_FUNCTION_OUTPUT_COMPARE;
  LedTimerChannel.ioType.outputAction = TIMER_OUTPUT_HIGH;
  LedTimerChannel.semaphore = OS_SemaphoreCreate(0);

  FTM_Set(&LedTimerChannel);

  for (uint8_t channelNb = 0; channelNb < ANALOG_NB_INPUTS; channelNb++)
  {
    AnalogProcessingThreadSettings[channelNb].Semaphore = OS_SemaphoreCreate(0);
    AnalogProcessingThreadSettings[channelNb].ChannelNb = channelNb;
    AnalogProcessingThreadSettings[channelNb].Values = &Analog_Input[channelNb];

    OS_ThreadCreate(AnalogProcessingThread,
        &AnalogProcessingThreadSettings[channelNb],
        &AnalogProcessingThreadStack[(channelNb + 1) * THREAD_STACK_SIZE - 1],
        10 - channelNb); /* Priority 10 & 9 */
  }

  OS_ThreadCreate(RTCTimerThread, NULL, &RTCThreadStack[THREAD_STACK_SIZE - 1], 6);
  OS_ThreadCreate(FTMLightThread, NULL, &FTMThreadStack[THREAD_STACK_SIZE - 1], 7);
  OS_ThreadCreate(ProtocolProcessingThread, NULL, &ProtocolProcessingThreadStack[THREAD_STACK_SIZE - 1], 1);

  // Start the PIT countdown
  // Will fire every 10ms
  PIT_Set(10000000, true);

  __EI();

  OS_Start();

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

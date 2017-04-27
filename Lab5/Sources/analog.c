/*! @brief Routines for setting up and reading from the ADC.
 *
 *  This contains the functions needed for accessing the external TWR-ADCDAC-LTC board.
 *
 *  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 *  @date 2016-10-06
 */
/*!
 * @addtogroup Analog_module Analog module documentation
 * @{
 */

#include "analog.h"
#include "median.h"
#include "SPI.h"
#include "MK70F12.h"
#include "PE_Types.h"

TAnalogInput Analog_Input[ANALOG_NB_INPUTS];

// ADC command masks
const uint8_t ADC_ODD_SHIFT = 6;
const uint8_t ADC_GAIN_MASK = 0x04;
const uint8_t ADC_SGL_MASK = 0x80;

// Address of the ADC slave
const uint8_t ADC_SLAVE_ADDR = 0x0F;

/*! @brief Wait for conversion function
 *
 *  Waits a sufficient amount of time for the SPI to change channels
 *
 */
static inline void WaitForConversion(void)
{
  for (uint16_t i = 0; i < 100; i++) { }
}

bool Analog_Init(const uint32_t moduleClock)
{
  // Create SPI Module struct
  TSPIModule spiModule;
  spiModule.isMaster = true;
  spiModule.continuousClock = false;
  spiModule.inactiveHighClock = false;
  spiModule.changedOnLeadingClockEdge = false; // Data is *captured* on leading edge
  spiModule.LSBFirst = false; // MSB first
  spiModule.baudRate = 1000 * 1000; // 1 Mb/s

  //Need to initialize the put ptr to the first value of the array
  for (uint8_t channelNb = 0; channelNb < ANALOG_NB_INPUTS; channelNb++)
  {
    Analog_Input[channelNb].putPtr = &(Analog_Input[channelNb].values[0]);
  }

  // Initialize Serial Peripheral Interface module
  return SPI_Init(&spiModule, moduleClock);
}

bool Analog_Get(const uint8_t channelNb)
{
  // Select the ADC device
  SPI_SelectSlaveDevice(ADC_SLAVE_ADDR);

  // Only channel 0 & 1 are supported
  if (channelNb >= ANALOG_NB_INPUTS)
    return false;

  TAnalogInput *input = &Analog_Input[channelNb];

  // Create ADC Command for switching mode
  uint16_t command =
                  (ADC_SGL_MASK | //Single sided diff - Not comparison
                   ADC_GAIN_MASK | //Range +-10V
                   (channelNb << ADC_ODD_SHIFT)) << 8; // Select the channelNb

  SPI_ExchangeChar(command, NULL);
  WaitForConversion();

  SPI_ExchangeChar(0, input->putPtr); // Read analog signal into current position
  WaitForConversion();

  // Increment the pointer, and loop back to start if needed.
  // Since we're running a median filter over the results we can use it as a
  // circular buffer
  input->putPtr++;
  if (input->putPtr == &input->values[ANALOG_WINDOW_SIZE])
    input->putPtr = &input->values[0];

  return true;
}

/*!
 * @}
 */

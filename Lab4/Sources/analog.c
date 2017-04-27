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


/*! @brief Selects the channel to use for our next data conversion
 *
 *  @param channelNb The number of the channel (0 or 1)
 */
static bool SelectChannel(uint8_t channelNb)
{
  // Only channel 0 & 1 are supported
  if (channelNb >= ANALOG_NB_INPUTS)
    return false;

  // Create ADC Command for switching mode
  uint16union_t command;
  command.s.Hi = ADC_SGL_MASK | //Singal sided diff - Not comparison
                  ADC_GAIN_MASK | //Range +-10V
                  (channelNb << ADC_ODD_SHIFT); // Select the channelNb

  // Send the ADC command
  SPI_ExchangeChar(command.l, NULL);

  // Wait for at least 4ns for conversion to complete
  for (uint16_t i = 1; i != 100; i++);

  return true;
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

  // Try switching to the appropriate channel
  if (!SelectChannel(channelNb))
    return false;

  TAnalogInput *input = &Analog_Input[channelNb];
  input->oldValue = input->value; // Set old value

  SPI_ExchangeChar(0, input->putPtr); // Read analog signal into current position

  // Increment the pointer, and loop back to start if needed.
  // Since we're running a median filter over the results we can use it as a
  // circular buffer
  input->putPtr++;
  if (input->putPtr == &input->values[ANALOG_WINDOW_SIZE])
    input->putPtr = &input->values[0];

  // Set value to the current median of "sliding window"
  input->value.l = Median_Filter(input->values, ANALOG_WINDOW_SIZE);

  // Dont forget to wait!
  return true;
}

/*!
 * @}
 */

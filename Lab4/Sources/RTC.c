/*! @file
 *
 *  @brief Routines for controlling the Real Time Clock (RTC) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the real time clock (RTC).
 *
 *  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 *  @date 2016-08-30
 */
/*!
 * @addtogroup RTC_module Real Time Clock module documentation
 * @{
 */
/* MODULE RTC */

#include "RTC.h"
#include "LEDs.h"
#include "MK70F12.h"
#include "PE_Types.h"

static void (*ptUserFunction)(void*); /*!< Consumer supplied function pointer to use when servicing interrupt */
static void* ptUserArguments; /*!< Consumer supplied arguments to pass into function when servicing interrupt */

bool RTC_Init(void (*userFunction)(void*), void* userArguments)
{
  // Check that the supplied callback function is valid
  if (userFunction == NULL)
    return false;

  // Set function and arguments
  ptUserFunction = userFunction;
  ptUserArguments = userArguments;

  // Enable RTC clock
  SIM_SCGC6 |= SIM_SCGC6_RTC_MASK;

  // Enable Time Seconds Interrupt (generates interrupt once a second)
  RTC_IER |= RTC_IER_TSIE_MASK;

  // Check if the oscillator needs to be enabled
  if (!(RTC_CR & RTC_CR_OSCE_MASK))
  {
    // Enable oscillator
    RTC_CR |= RTC_CR_OSCE_MASK;

    // After setting this bit, wait the oscillator startup time before enabling
    // the time counter to allow the 32.768 kHz clock time to stabilise
    for (uint32_t i = 0; i < 0x60000; i++) ; // Arbitrary, should be fine
  }

  // Check is time invalid flag is set (happens on software reset)
  if (RTC_SR & RTC_SR_TIF_MASK)
  {
    // Clear TIF flag by writing to TSR (by setting time)
    RTC_Set(0, 0, 1); // Writing to TSR with zero is supported, but not recommended
  }

  return true;
}

void RTC_Set(const uint8_t hours, const uint8_t minutes, const uint8_t seconds)
{
  // Reset prescaler to default
  RTC_TPR &= ~RTC_TPR_TPR_MASK;

  // Disable Time Counter to allow setting time
  RTC_SR &= ~RTC_SR_TCE_MASK;

  // Set time seconds register
  // ASSUME all parameters are in range as per header file
  RTC_TSR = (hours * 60 * 60) +
            (minutes * 60) +
            seconds;

  // Re enable Time Counter
  RTC_SR |= RTC_SR_TCE_MASK;
}

void RTC_Get(uint8_t* const hours, uint8_t* const minutes,
    uint8_t* const seconds)
{
  // Get time (in seconds) out of time seconds register
  uint32_t time = RTC_TSR;

  *hours = time / (60 * 60); // Set hours output to hour component of TSR
  time %= 60 * 60; // Remove hour component

  *minutes = time / 60; // Set minutes output to minute component of TSR
  time %= 60; // Remove minute component

  *seconds = time; // Set seconds output to seconds component of TSR
}

void __attribute__ ((interrupt)) RTC_ISR(void)
{
  // Call supplied function with supplied arguments
  ptUserFunction(ptUserArguments);
}

/*!
 * @}
 */

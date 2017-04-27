/*! @file
 *
 *  @brief Routines for controlling Periodic Interrupt Timer (PIT) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the periodic interrupt timer (PIT).
 *
 *  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 *  @date 2016-08-30
 */
/*!
 * @addtogroup PIT_module Periodic Interrupt Timer module documentation
 * @{
 */
/* MODULE PIT */

#include "PIT.h"
#include "Cpu.h"
#include "MK70F12.h"

static void (*ptUserFunction)(void*); /*!< Consumer supplied function pointer to use when servicing interrupt */
static void* ptUserArguments; /*!< Consumer supplied arguments to pass into function when servicing interrupt */
static uint32_t timerPeriodNs; /*!< Period of the timer in nanoseconds */

bool PIT_Init(const uint32_t moduleClk, void (*userFunction)(void*), void* userArguments)
{
  // Check that the supplied callback function is valid
  if (userFunction == NULL)
    return false;

  // Set function and arguments
  ptUserFunction = userFunction;
  ptUserArguments = userArguments;

  // Period = 1s / Freq
  //        = 1,000,000,000ns / Freq
  timerPeriodNs = 1000000000 / moduleClk;

  // Enable PIT clock
  SIM_SCGC6 |= SIM_SCGC6_PIT_MASK;

  // Module Disable.
  // This field must be enabled before any other setup is done
  PIT_MCR &= ~PIT_MCR_MDIS_MASK; // 0 = Enabled
  PIT_MCR |= PIT_MCR_FRZ_MASK; // 1 = Enable

  // Enable interrupt
  PIT_TCTRL0 |= PIT_TCTRL_TIE_MASK;

  return true;
}

void PIT_Set(const uint32_t period, const bool restart)
{
  if (restart)
  {
    // To abort the current cycle and start a timer period with the new
    // value, the timer must be disabled and enabled again
    PIT_Enable(false);
  }

  // Set Timer Start Value (timer counts down and triggers an interrupt)
  PIT_LDVAL0 = period / timerPeriodNs;

  // "The function will enable the timer and interrupts for the PIT."
  PIT_Enable(true);
}

void PIT_Enable(const bool enable)
{
  if (enable)
    // Set Timer Enable bit on Timer Control Register
    PIT_TCTRL0 |= PIT_TCTRL_TEN_MASK;
  else
    // Clear Timer Enable bit on Timer Control Register
    PIT_TCTRL0 &= ~PIT_TCTRL_TEN_MASK;
}

void __attribute__ ((interrupt)) PIT_ISR(void)
{
  // Clear the interrupt flag, otherwise we always interrupt eternally!
  PIT_TFLG0 |= PIT_TFLG_TIF_MASK; // w1c

  // Call supplied function with supplied arguments
  ptUserFunction(ptUserArguments);
}

/*!
 * @}
 */

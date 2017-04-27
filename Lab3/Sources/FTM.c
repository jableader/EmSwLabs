/*! @file
 *
 *  @brief Routines for setting up the FlexTimer module (FTM) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the FlexTimer module (FTM).
 *
 *  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 *  @date 2016-08-31
 */
/*!
 * @addtogroup FTM_module FlexTimer module documentation
 * @{
 */
/* MODULE FTM */

#include "FTM.h"
#include "MK70F12.h"
#include "Cpu.h"

// Struct encapsulating the user supplied function pointer and arguments
typedef struct
{
  void (*userFunction)(void*); /*!< Consumer supplied function pointer to use when servicing interrupt */
  void *userArguments; /*!< Consumer supplied arguments to pass into function when servicing interrupt */
} TUserCallback;

static TUserCallback userCallbacks[8]; /*!< Call backs for all 8 FTM channels */

bool FTM_Init(void)
{
  // Enable clock for FTM module
  SIM_SCGC6 |= SIM_SCGC6_FTM0_MASK;

  // Disable Write protection
  FTM0_MODE |= FTM_MODE_WPDIS_MASK;

  FTM0_CNTIN = 0; // Set initial value to 0
  FTM0_MOD = 0xFFFF; // Modulo register, set high as we will not be relying on overflow interrupts
  FTM0_CNT = 0; // Counter: Writing any value will reset to CNTIN

  // Set clock source
  FTM0_SC |= FTM_SC_CLKS(2); // 0b10 = Fixed frequency clock

  // Enable FlexTimer Module
  FTM0_MODE |= FTM_MODE_FTMEN_MASK;

  return true;
}

bool FTM_Set(const TFTMChannel* const aFTMChannel)
{
  // Check a valid channel was selected
  if (aFTMChannel->channelNb > 7)
    return false;

  // Set callback function + arguments
  TUserCallback callback = { aFTMChannel->userFunction, aFTMChannel->userArguments };
  userCallbacks[aFTMChannel->channelNb] = callback;

  // Get pointer to Channel (n) Status and Control
  volatile uint32_t *cnsc = &FTM0_CnSC(aFTMChannel->channelNb);

  // Set input detection (Edge Or Level Select: ELSA, ELSB)
  *cnsc |= (uint8_t) (aFTMChannel->ioType.inputDetection) << 2;

  // Set timer function (Channel Mode Select: MSA, MSB)
  // i.e. Input Capture or Output Compare
  *cnsc |= (uint8_t) (aFTMChannel->timerFunction) << 4;

  *cnsc &= ~FTM_CnSC_CHF_MASK; // Clear interrupt flag on Channel N
  *cnsc |= FTM_CnSC_CHIE_MASK; // Enable interrupts on Channel N

  return true;
}

bool FTM_StartTimer(const TFTMChannel* const aFTMChannel)
{
  // Validate that the channel is for Output Compare
  if (aFTMChannel->timerFunction != TIMER_FUNCTION_OUTPUT_COMPARE)
    return false;

  // Setup channel
  if (FTM_Set(aFTMChannel))
  {
    // Set compare register to Current Value + Required Delay
    FTM0_CnV(aFTMChannel->channelNb) = FTM0_CNT + aFTMChannel->delayCount;
    return true;
  }

  // Channel was not setup correctly
  return false;
}

void __attribute__ ((interrupt)) FTM0_ISR(void)
{
  // Loop through all channels to find an interrupt to service
  for (uint8_t i = 0; i < 8; i++)
  {
    // Find the first channel to service (with interrupt flag set)
    volatile uint32_t *cnsc = &FTM0_CnSC(i);
    if ((*cnsc & FTM_CnSC_CHIE_MASK) && (*cnsc & FTM_CnSC_CHF_MASK))
    {
      // Clear interrupt flag
      *cnsc &= ~FTM_CnSC_CHF_MASK;

      // Check a callback function has been set
      if (userCallbacks[i].userFunction != NULL)
      {
        // Call supplied function with supplied arguments
        userCallbacks[i].userFunction(userCallbacks[i].userArguments);

        // Detach the channel from the timer
        *cnsc &= ~(FTM_CnSC_ELSA_MASK | FTM_CnSC_ELSB_MASK);

        break; // Only need to service one channel per interrupt
      }
    }
  }
}

/*!
 * @}
 */

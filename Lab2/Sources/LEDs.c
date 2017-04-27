/*! @file
 *
 *  @brief Routines to access the LEDs on the TWR-K70F120M.
 *
 *  This contains the functions for operating the LEDs.
 *
 *  Created in Kinetis Design Studio 3.2.0 for the TWR-K70F120M (MK70FN1M0VMJ12 microcontroller)
 *
 *  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 *  @date 2016-08-22
 */

#include "LEDs.h"
#include "MK70F12.h"

// Macro for a mask with all LED control bits set
#define ALL_LEDS (LED_ORANGE | LED_YELLOW | LED_GREEN | LED_BLUE)

bool LEDs_Init(void)
{
  // Enable PORT A clock
  SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK;

  // Drive Strength Enabled = 1
  PORTA_PCR11 |= PORT_PCR_DSE_MASK; // Orange
  PORTA_PCR28 |= PORT_PCR_DSE_MASK; // Yellow
  PORTA_PCR29 |= PORT_PCR_DSE_MASK; // Green
  PORTA_PCR10 |= PORT_PCR_DSE_MASK; // Blue

  // Configure multiplexed PINs in Port A for GPIO OUT usage
  PORTA_PCR11 |= PORT_PCR_MUX(1); // Orange
  PORTA_PCR28 |= PORT_PCR_MUX(1); // Yellow
  PORTA_PCR29 |= PORT_PCR_MUX(1); // Green
  PORTA_PCR10 |= PORT_PCR_MUX(1); // Blue

  //Set the LEDs pins to all be OUTPUT pins
  GPIOA_PDDR |= ALL_LEDS;

  // Switch the LEDs off as default
  LEDs_Off(ALL_LEDS);

  return true;
}

void LEDs_On(const TLED color)
{
  GPIOA_PCOR = color;
}

void LEDs_Off(const TLED color)
{
  GPIOA_PSOR = color;
}

void LEDs_Toggle(const TLED color)
{
  GPIOA_PTOR = color;
}

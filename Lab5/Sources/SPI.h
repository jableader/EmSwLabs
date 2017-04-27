/*! @file
 *
 *  @brief I/O routines for K70 SPI interface.
 *
 *  This contains the functions for operating the SPI (serial peripheral interface) module.
 *
 *  @author PMcL
 *  @date 2016-08-22
 */

#ifndef SPI_H
#define SPI_H

// new types
#include "types.h"

typedef struct
{
  bool isMaster;                   /*!< A Boolean value indicating whether the SPI is master or slave. */
  bool continuousClock;            /*!< A Boolean value indicating whether the clock is continuous. */
  bool inactiveHighClock;          /*!< A Boolean value indicating whether the clock is inactive low or inactive high. */
  bool changedOnLeadingClockEdge;  /*!< A Boolean value indicating whether the data is clocked on even or odd edges. */
  bool LSBFirst;                   /*!< A Boolean value indicating whether the data is transferred LSB first or MSB first. */
  uint32_t baudRate;               /*!< The baud rate in bits/sec of the SPI clock. */
} TSPIModule;

/*! @brief Sets up the SPI before first use.
 *
 *  @param aSPIModule is a structure containing the operating conditions for the module.
 *  @param moduleClock The module clock in Hz.
 *  @return BOOL - true if the SPI module was successfully initialized.
 */
bool SPI_Init(const TSPIModule* const aSPIModule, const uint32_t moduleClock);

/*! @brief Selects the current slave device
 *
 * @param slaveAddress The slave device address.
 * The lower two bits represent the SPI's Chip Select.
 * The third and fourth bits represent the 5th & 27th E pins, which
 * are connected to the GPIO 7 line of the tower.
 */
void SPI_SelectSlaveDevice(const uint8_t slaveAddress);

/*! @brief Transmits a byte and retrieves a received byte from the SPI.
 *
 *  @param dataTx is a byte to transmit.
 *  @param dataRx points to where the received byte will be stored.
 */
void SPI_ExchangeChar(const uint16_t dataTx, uint16_t* const dataRx);

#endif

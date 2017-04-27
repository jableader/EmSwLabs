/*! @file
 *
 *  @brief I/O routines for K70 SPI interface.
 *
 *  This contains the functions for operating the SPI (serial peripheral interface) module.
 *
 *  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 *  @date 2016-08-22
 */

#include <stdlib.h>
#include "SPI.h"
#include "MK70F12.h"
#include "Cpu.h"

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof(array[0]))

// The value to use as the chip select. Can be set through SPI_SelectSlaveDevice
static uint8_t slaveAddressCS = 0;

/*! @brief Calculates the prescaler values needed to meet the desired baud rate.
 *
 *  @param moduleClock The module clock in Hz
 *  @param targetBaudRate The desired baud rate in bits/s.
 *  @param dbr A pointer to place the calculated double baud rate value in (0 or 1)
 *  @param pbr A pointer to place the calculated baud rate prescaler value in (0 to 3)
 *  @param br A pointer to place the calculated baud rate value in (0 to 15)
 *  @return BOOL - true if the prescalers could be calculated. Currently always true
 */
static bool CalculatePrescalers(const uint32_t moduleClock, const uint32_t targetBaudRate, uint8_t* dbr, uint8_t* pbr, uint8_t* br)
{
  // See page 1836 in K70F3RM document
  // Create arrays of candidate scaler values
  // Indexes correspond with tables in the datasheet, so index can be used to directly set the registers
  const int32_t DBR_VALUES[2] = { 0, 1 }; // Double baud rate
  const int32_t PBR_VALUES[4] = { 2, 3, 5, 7 }; // Baud Rate Prescaler
  const int32_t BR_VALUES[16] = { 2, 4, 6, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768 }; // Baud Rate scaler

  // Calculate the division factor that needs to be applied to the module clock to achieve the target baud rate
  int32_t targetDivisor = moduleClock / targetBaudRate;

  // Parameters used store best current parameters
  int32_t bestDbr, bestPbr, bestBr, bestDivisor;

  // Exhaustively search for the best scalers to use to find a baud rate similar to the target
  for (int32_t i = 0; i < ARRAY_LENGTH(DBR_VALUES); i++)
  {
    int32_t candidateDbr = DBR_VALUES[i];

    for (int32_t j = 0; j < ARRAY_LENGTH(PBR_VALUES); j++)
    {
      int32_t candidatePbr = PBR_VALUES[j];

      for (int32_t k = 0; k < ARRAY_LENGTH(BR_VALUES); k++)
      {
        int32_t candidateBr = BR_VALUES[k];

        // Calculate the effective divisor that will be applied to the module clock
        int32_t currentDivisor = (candidatePbr * (candidateBr / (1 + candidateDbr)));

        // Check if the current values are better than our best result so far
        if (abs(targetDivisor - currentDivisor)	< abs(targetDivisor - bestDivisor))
        {
          // Found new potential baud rate - set best result fields
          bestDivisor = currentDivisor;
          bestDbr = i;
          bestPbr = j;
          bestBr = k;
        }
      }
    }
  }

  // Output best parameters
  *dbr = bestDbr;
  *pbr = bestPbr;
  *br = bestBr;

  // Leave interface open for modification, may want to validate the baud rate
  // is within a given tolerance later
  return true;
}

/*!
 * @addtogroup RTC_module Real Time Clock module documentation
 * @{
 */

bool SPI_Init(const TSPIModule* const aSPIModule, const uint32_t moduleClock)
{
  const int FRAME_SIZE = 16;

  // Enable gated clocks
  SIM_SCGC5 |= SIM_SCGC5_PORTE_MASK; // Enable PORT E clock
  SIM_SCGC3 |= SIM_SCGC3_DSPI2_MASK; // Enable SPI2 clock
  SIM_SCGC5 |= SIM_SCGC5_PORTD_MASK; // Enable PORT D clock

  // Drive Strength Enabled = 1
  PORTE_PCR5 |= PORT_PCR_DSE_MASK;
  PORTE_PCR27 |= PORT_PCR_DSE_MASK;

  // Configure multiplexed PINs in Port E for GPIO OUT usage
  PORTE_PCR27 |= PORT_PCR_MUX(1);
  PORTE_PCR5 |= PORT_PCR_MUX(1);

  // Set both slave select pins as output pins
  GPIOE_PDDR |= (1 << 27) | (1 << 5);

  // Enable all multiplexed SPI2 pins
  PORTD_PCR12 |= PORT_PCR_MUX(2); // SPI2_SCK
  PORTD_PCR13 |= PORT_PCR_MUX(2); // SPI2_SOUT
  PORTD_PCR14 |= PORT_PCR_MUX(2); // SPI2_SIN
  PORTD_PCR11 |= PORT_PCR_MUX(2); // SPI2_PCS0
  PORTD_PCR15 |= PORT_PCR_MUX(2); // SPI2_PCS1

  SPI2_MCR &= ~SPI_MCR_DCONF_MASK;	// Select SPI configuration
  SPI2_MCR &= ~SPI_MCR_MTFE_MASK;		// Modified transfer format disabled
  SPI2_MCR &= ~SPI_MCR_PCSSE_MASK;	// PCSS5 is used as the peripheral chip select signal
  SPI2_MCR &= ~SPI_MCR_ROOE_MASK;		// Recieve overflow overwrite, incoming data is ignored
  SPI2_MCR |= SPI_MCR_FRZ_MASK; 		// Halt transfers in debug mode
  SPI2_MCR &= ~SPI_MCR_DOZE_MASK;		// Switch off DOZE
  SPI2_MCR &= ~SPI_MCR_MDIS_MASK;		// Enable module clock

  // Disable recieve/trsnsmit FIFOs
  SPI2_MCR |= SPI_MCR_DIS_TXF_MASK;
  SPI2_MCR |= SPI_MCR_DIS_RXF_MASK;

  // Keep the chip select line high for the last line
  SPI2_MCR |= SPI_MCR_PCSIS(1);

  // Set Master/Slave Mode Select
  if (aSPIModule->isMaster)
    SPI2_MCR |= SPI_MCR_MSTR_MASK;

  // Set continuous clock enable
  if (aSPIModule->continuousClock)
    SPI2_MCR |= SPI_MCR_CONT_SCKE_MASK;

  // Set Clock and Transfer Attributes Register (CTAR)
  if (aSPIModule->isMaster)
  {
    // Calculate Baud Rate scalers
    uint8_t dbr, pbr, br;

    if (!CalculatePrescalers(moduleClock, aSPIModule->baudRate, &dbr, &pbr,	&br))
      return false; // Could not generate baud rate, do not continue

    // Set Frame Size (bits transferred = FMSZ + 1)
    // Intentionally set first, FMSZ is the only non zero value on startup
    SPI2_CTAR0 = SPI_CTAR_FMSZ(FRAME_SIZE - 1);

    // Set Baud Rate Values
    SPI2_CTAR0 |= (dbr << SPI_CTAR_DBR_SHIFT);
    SPI2_CTAR0 |= SPI_CTAR_PBR(pbr);
    SPI2_CTAR0 |= SPI_CTAR_BR(br);

    // Set LSB First
    if (aSPIModule->LSBFirst)
      SPI2_CTAR0 |= SPI_CTAR_LSBFE_MASK;

    // Set Clock Phase
    if (aSPIModule->changedOnLeadingClockEdge)
      SPI2_CTAR0 |= SPI_CTAR_CPHA_MASK;

    // Set Clock Polarity
    if (aSPIModule->inactiveHighClock)
      SPI2_CTAR0 |= SPI_CTAR_CPOL_MASK;
  }
  else
  {
    // Set Frame Size (bits transferred = FMSZ + 1)
    SPI2_CTAR0_SLAVE = SPI_CTAR_FMSZ(FRAME_SIZE - 1);

    // Set Clock Phase
    if (aSPIModule->changedOnLeadingClockEdge)
      SPI2_CTAR0_SLAVE |= SPI_CTAR_CPHA_MASK;

    // Set Clock Polarity
    if (aSPIModule->inactiveHighClock)
      SPI2_CTAR0_SLAVE |= SPI_CTAR_CPOL_MASK;
  }

  // Clear HALT bit - starts frame transfers
  SPI2_MCR &= ~SPI_MCR_HALT_MASK;

  return true;
}

void SPI_SelectSlaveDevice(const uint8_t slaveAddress)
{
  // The lower two bits represent what weant our chip select to be
  slaveAddressCS = 0x03 & slaveAddress;

  // The third bit represents what pin E5 should be
  if (slaveAddress & 0x4)
  {
    GPIOE_PSOR |= (1 << 5);
  }
  else
  {
    GPIOE_PCOR |= (1 << 5);
  }

  // The fourth bit represents what pin E5 should be
  if (slaveAddress & 0x8)
  {
    GPIOE_PSOR |= (1 << 27);
  }
  else
  {
    GPIOE_PCOR |= (1 << 27);
  }
}

void SPI_ExchangeChar(const uint16_t dataTx, uint16_t* const dataRx)
{
  // Wait until SPI is ready for transmission
  while (!(SPI2_SR & SPI_SR_TFFF_MASK));

  // Place data into the output buffer, including the correct chip select
  SPI2_PUSHR = SPI_PUSHR_TXDATA(dataTx) | SPI_PUSHR_PCS(slaveAddressCS);

  // Wait for transmission/reception to finish
  while (!(SPI2_SR & SPI_SR_RFDF_MASK));

  // Extract the recieved data from the input buffer
  if (dataRx != NULL)
  {
    *dataRx = SPI2_POPR;
  }

  // Clear all the flags
  SPI2_SR = SPI_SR_TCF_MASK | SPI_SR_RFDF_MASK | SPI_SR_TFFF_MASK;
}

/*!
 * @}
 */

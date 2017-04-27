/*! @file
 *
 *  @brief I/O for UART communications on the TWR-K70F120M
 *
 *  This contains the implementation for interfacing with the UART (serial port) on the TWR-K70F120M.
 *
 *  Created in Kinetis Design Studio 3.2.0 for the TWR-K70F120M (MK70FN1M0VMJ12 microcontroller)
 *
 *  @author Group 13, Jacob Dunk (11654718) & Brenton Smith (11380654)
 *  @date 2016-08-14
 */

#include "UART.h"
#include "FIFO.h"
#include "MK70F12.h"

#define UART2_RDRF (UART2_S1 & UART_S1_RDRF_MASK) // UART2 Receive Data Register Full Flag Mask
#define UART2_TDRE (UART2_S1 & UART_S1_TDRE_MASK) // UART2 Transmit Data Register Empty Flag Mask

static TFIFO  TxFIFO,  /*!< The Transmit FIFO Buffer */
              RxFIFO;  /*!< The Receive FIFO Buffer */

bool UART_Init(const uint32_t baudRate, const uint32_t moduleClk)
{
  // Initialise the circular FIFO buffers for Received and Transmitted data
  FIFO_Init(&TxFIFO); // Initialise the Transmit FIFO
  FIFO_Init(&RxFIFO); // Initialise the Receive FIFO

  // Enable UART2
  SIM_SCGC4 |= SIM_SCGC4_UART2_MASK;

  // Enable Pin Routing for Port E
  SIM_SCGC5 |= SIM_SCGC5_PORTE_MASK;

  // Configure multiplexed PINs Port E 16, 17 for UART2 usage (K70P256M150SF3RM.pdf p. 280)
  PORTE_PCR16 = PORT_PCR_MUX(3); // UART2_TX = PTE16 ALT3
  PORTE_PCR17 = PORT_PCR_MUX(3); // UART2_RX = PTE17 ALT3

  // Generate / Set baud rate (K70P256M150SF3RM.pdf, p. 1973)

  // UART baud rate = UART module clock / (16 × (SBR[12:0] + BRFD))
  // 16 × (SBR[12:0] + BRFD) =  UART module clock / UART baud rate
  //
  // BRFD < 1 and will be represented as a 5 bit integer (0-31). Multiply by 2 to avoid floating point calculations.
  // 32 × (SBR[12:0] + BRFD) =  2 x (UART module clock / UART baud rate)
  const uint32_t baudRateDivisor = 2 * (moduleClk / baudRate); // = 32 × (SBR[12:0] + BRFD)

  // Split SBR and BRFD, as BRFD < 1 we can use integer division to split
  // baudRateDivisor = 32 × (SBR[12:0] + BRFD), SBR > 1 and BRFD < 1
  const uint16_t sbr = baudRateDivisor / 32;
  const uint16_t brfd = baudRateDivisor % 32;

  // Set baud rate divisor 13-bit modulus counter
  UART2_BDH = (uint8_t) ((sbr >> 8) & 0x1F); // Set 5 high bits
  UART2_BDL = (uint8_t) sbr; // Set 8 Low bits

  // Set fine adjust 5 bits
  UART2_C4 |= brfd & UART_C4_BRFA_MASK;

  UART2_C1 &= ~UART_C1_LOOPS_MASK; // LOOPS - Normal mode
  UART2_C1 &= ~UART_C1_UARTSWAI_MASK; // UART clock continues to run in Wait mode
  UART2_C1 &= ~UART_C1_RSRC_MASK; // Internal Loop Back Mode
  UART2_C1 &= ~UART_C1_M_MASK; // Normal 8 bit mode
  UART2_C1 &= ~UART_C1_WAKE_MASK; // Idle Line Wakeup
  UART2_C1 &= ~UART_C1_ILT_MASK; // Idle character bit count starts after start bit
  UART2_C1 &= ~UART_C1_PE_MASK; // Parity function disabled
  UART2_C1 &= ~UART_C1_PT_MASK; // Even parity

  UART2_C2 &= ~UART_C2_TIE_MASK; // Disable Transmit Empty Interrupts
  UART2_C2 &= ~UART_C2_TCIE_MASK; // Disable Transmit Complete Interrupts
  UART2_C2 &= ~UART_C2_RIE_MASK; // Disable Recieve Full Interrupts
  UART2_C2 &= ~UART_C2_ILIE_MASK; // Disable Idle Line Interrupts
  UART2_C2 &= ~UART_C2_RIE_MASK; // Disable Recieve Full Interrupts
  UART2_C2 |= UART_C2_TE_MASK; // Enable Transmit
  UART2_C2 |= UART_C2_RE_MASK; // Enable Recieve
  UART2_C2 &= ~UART_C2_RWU_MASK; // Reciever Wakeup - Normal Mode
  UART2_C2 &= ~UART_C2_SBK_MASK; // Send Break - Normal Mode

  return true;
}

bool UART_InChar(uint8_t * const dataPtr)
{
  // Get data from the received FIFO buffer
  return FIFO_Get(&RxFIFO, dataPtr);
}

bool UART_OutChar(const uint8_t data)
{
  // Add data to the transmit FIFO buffer
  return FIFO_Put(&TxFIFO, data);
}

void UART_Poll(void)
{
  uint8_t txData;

  // Check if data ready to read from UART2
  if (UART2_RDRF)
  {
    // Read from UART2 data register into the receive FIFO buffer
    FIFO_Put(&RxFIFO, UART2_D);
  }

  // Check if UART2 is ready to transmit and there is data waiting in transmit FIFO buffer
  if (UART2_TDRE && FIFO_Get(&TxFIFO, &txData))
  {
    // Write to UART2 data register
    UART2_D = txData;
  }
}

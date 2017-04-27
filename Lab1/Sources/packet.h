/*! @file
 *
 *  @brief Routines to implement packet encoding and decoding for the serial port.
 *
 *  This contains the functions for implementing the "Tower to PC Protocol" 5-byte packets.
 *
 *  @author PMcL
 *  @date 2015-07-23
 */

#ifndef PACKET_H
#define PACKET_H

// new types
#include "types.h"
#include "UART.h"

// Packet structure
extern uint8_t 	Packet_Command,		/*!< The packet's command */
		Packet_Parameter1, 	/*!< The packet's 1st parameter */
		Packet_Parameter2, 	/*!< The packet's 2nd parameter */
		Packet_Parameter3;	/*!< The packet's 3rdt parameter */

// Acknowledgement bit mask
extern const uint8_t PACKET_ACK_MASK;

/*! @brief Initializes the packets by calling the initialization routines of the supporting software modules.
 *
 *  @param baudRate The desired baud rate in bits/sec.
 *  @param moduleClk The module clock rate in Hz
 *  @return BOOL - TRUE if the packet module was successfully initialized.
 */
BOOL Packet_Init(const uint32_t baudRate, const uint32_t moduleClk);

/*! @brief Attempts to get a packet from the received data.
 *
 *  @return BOOL - TRUE if a valid packet was received.
 */
BOOL Packet_Get(void);

/*! @brief Builds a packet and places it in the transmit FIFO buffer.
 *
 *  @return BOOL - TRUE if a valid packet was sent.
 */
BOOL Packet_Put(const uint8_t command, const uint8_t parameter1, const uint8_t parameter2, const uint8_t parameter3);

#endif

/*
 * midi_uart.h
 *
 *  Created on: Jan 19, 2020
 *      Author: andy
 *
 *  Mod:
 *  2020-01-20 andy. Add structures and typedefs for the MIDI port and for the receive state machine.
 *  2020-01-21 andy. The midiport_t structure now has the transmit ring buffer included.
 *  2020-01-29 andy. message FIFO has only head and tail pointers, we no longer maintain
 *                      a separate count.
 */

#ifndef MIDI_UART_MIDI_UART_H_
#define MIDI_UART_MIDI_UART_H_

#include <stdint.h>
#include <stdbool.h>
#include "midi.h"
#include "usbmidi_types.h"

/**
 * Size of the message transmit FIFO, in bytes.
 */
#ifndef MIDI_TX_FIFO_SIZE
#define MIDI_TX_FIFO_SIZE 64
#endif

/**
  *  \enum MIDIUART_rxstate_t
  *  Define states in the receiver state machine.
  */
typedef enum {
    MU_IDLE,        //!< waiting to start a new packet
    MU_DATABYTE2,   //!< next byte in will be for mep->byte2
    MU_DATABYTE3,   //!< next byte in will be for mep->byte3
    MU_SYSEX1,      //!< next byte in is 1st sysex byte, put in mep->byte2
    MU_SYSEX2       //!< next byte in is 2nd sysex byte or EOX, put in mep->byte3
} MIDIUART_rxstate_t; //!< receive state register.

/**
 * This structure contains all of the status and control information
 * needed by a particular serial MIDI port.
 * A pointer to this stucture is passed to the UART MIDI functions.
 * Instances of this structure should be declared as volatile.
 */
typedef struct
{
	/* Configuration */
    uint32_t uartbase;            //!< base address of the UART peripheral used by this port
    uint32_t uartint;             //!< NVIC entry for this UART's interrupt.
    uint8_t cablenum;             //!< cable number of this port, used for USB-MIDI connections

    // "private" members, do not change from user code. These is for the receiver.
    uint8_t cin;                  //!< Code Index Number for this packet
    uint8_t bytecnt;              //!< iterator for data bytes in this packet
    uint8_t bytesinpacket;        //!< set by status parser for running status.
    MIDIUART_rxstate_t rxstate;   //!< state register

    // ... and these are for the transmitter.
    uint8_t txmsgfifo[MIDI_TX_FIFO_SIZE];	//!< Transmit fifo buffer
    uint8_t txfifohead;			  //!< write location
    uint8_t txfifotail;			  //!< read location
    uint8_t txidle;               //!< true when idle
} midiport_t;

/**
 * Set up the serial port for MIDI operation.
 * This populates the port structure with the necessary details.
 * @param port is the port structure that should be passed to all functions.
 * @param uartbase is the base address of the UART's register space
 * @param scperiph is the corresponding peripheral number passed to SysCtl functions
 * @param sysclkfreq is the clock frequency as set by SysCtlClockFreqSet().
 * @param cablenum is the USB "cable number" we use to distinguish this port from the USB perspective
 */
void MIDIUART_Init(midiport_t *port, uint32_t uartbase, uint32_t scperiph, uint32_t sysclkfreq, uint8_t cablenum, uint32_t intnum);

/**
 * Write the given message to the transmit message FIFO.
 * @param port is the structure for this port.
 * @param msg is a pointer to an array of bytes to write to the FIFO.
 * @param msize is the number of bytes in this message to write to the FIFO.
 *
 * This function will block if there is no room in the FIFO for the message.
 */
void MIDIUART_writeMessage(midiport_t *port, uint8_t *msg, uint8_t msize);

/**
 * Attempt to read a message that was received on the serial MIDI port.
 * Pass a pointer to the structure that will hold the received message.
 * When a complete message has been received, this function returns true
 * and the message will be in that structure.
 *
 * @param[in,out] port
 * @param[out] msg
 */
bool MIDIUART_readMessage(midiport_t *port, USBMIDI_Message_t *msg);




#endif /* MIDI_UART_MIDI_UART_H_ */

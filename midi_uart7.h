/*
 * midi_uart7.h
 *
 *  Created on: Jan 27, 2020
 *      Author: apeters
 *
 * Most of the UART MIDI code can be abstracted and shared so we can support
 * more than one serial MIDI port in a design.
 *
 * The structure for this instance of a serial MIDI port is defined here.
 */

#ifndef MIDI_UART7_H_
#define MIDI_UART7_H_

/**
 * Declare an instance of the midiport_t structure used for this port.
 * It will be initialized in the call to MIDIUART_Init().
 * This port is now visible to whatever code includes this header.
 */
#include "midi_uart.h"
extern midiport_t mpuart7;


#endif /* MIDI_UART7_H_ */

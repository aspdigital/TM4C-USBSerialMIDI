/*
 * midi_usb_rx_task.c
 *
 *  Created on: Jul 28, 2020
 *      Author: andy
 */

#include <stdint.h>
#include <stdbool.h>

#include "utils/uartstdio.h"
#include "utils/ustdlib.h"

#include "usb_midi.h"
#include "usb_midi_fifo.h"
#include "midi_usb_rx_task.h"
#include "usbmidi.h"

/**
 * Pop the USB OUT receive FIFO for as long as there is something to pop.
 */
void MIDI_USB_Rx_Task(void)
{
	USBMIDI_Message_t msg;

	while( USBMIDI_OutEpFIFO_Pop(&msg) )
	{
		UARTprintf("%02x : %02x : %02x : %02x\n", msg.header, msg.byte1, msg.byte2, msg.byte3);
	}
}




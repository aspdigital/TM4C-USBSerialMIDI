/*
 * usb_midi.h
 *
 *  Created on: Jan 31, 2020
 *      Author: andy
 *
 * Includes some definitions that are not part of what TI offers.
 */

#ifndef USB_MIDI_USBMIDI_TYPES_H_
#define USB_MIDI_USBMIDI_TYPES_H_

#include "usblib/usblib.h"
#include "usblib/device/usbdevice.h"
#include "usb_midi_fifo.h"


/**
 * Size of receive and transmit buffers.
 * Chosen pretty much at random but should be at least 2x the size of a max-sized USB packet.
 */
#define USB_BUFFER_SIZE (256)

/**
 * status of the two directions.
 */
typedef enum
{
	eUsbMidiStateUnconfigured, 	// not configured
	eUsbMidiStateIdle,			// no outstanding transaction remains to be completed
	eUsbMidiStateWaitData		// waiting on completion of a send or receive transaction
} tUSBMidiState;

/**
 * This is the "Device instance" structure, used for ...
 */
typedef struct
{
	// Base address of USB hardware in the micro, There's only one,
	// so maybe this can be deleted?
	uint32_t ui32USBBase;

	// lower level DCD code needs this?
	tDeviceInfo sDevInfo;

	// state of receive channel
	volatile tUSBMidiState iUSBMidiRxState;

	// state of transmit channel
	volatile tUSBMidiState iUSBMidiTxState;

	// device connection status.
	volatile bool bConnected;

} tUSBMidiInstance;

/**
 * This is the "device structure."
 * Its main purpose is to hold the USB Buffer callback functions and data.
 * Its private structure has the low-level stuff (see above).
 */
typedef struct
{
	USBMIDIFIFO_t InEpMsgFifo;
	USBMIDIFIFO_t OutEpMsgFifo;
	tUSBMidiInstance sPrivateData;

} tUSBMidiDevice;


#endif /* USB_MIDI_USBMIDI_TYPES_H_ */

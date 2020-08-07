/**
 * usb_midi.c
 *
 *  Created on: Oct 28, 2019
 *      Author: apeters
 *
 *  Mod 2019-10-30 asp. Declare the receive USB (OUT) MIDI message FIFO here.
 *  	Also use a FIFO for USB IN (write to host) messages.
 */
#include "em_usb.h"
#include "usbconfig.h"
#include "usb_midi.h"
#include "usb_midi_fifo.h"

/**
 * Endpoint buffer for USB BULK OUT (receive) transfers for the USB MIDI function.
 * It is the same size as the endpoint's max packet size, and will hold one or
 * more MIDI Event Packets at the time the receive callback is invoked.
 * The address of this buffer is passed in the call to USBD_Read() for this endpoint.
 */
static USBMIDI_Message_t OutEpBuf[USB_FS_BULK_EP_MAXSIZE/sizeof(USBMIDI_Message_t)] SL_ATTRIBUTE_ALIGN(4);

/**
 * Endpoint buffer for USB BULK IN (transmit) transfers for the USB MIDI function.
 * It is the same size as the endpoint's max packet size.
 * In the transfer-complete callback, we will load this buffer with one or more
 * MIDI messages by popping the transmit FIFO.
 */
static USBMIDI_Message_t InEpBuf[USB_FS_BULK_EP_MAXSIZE/sizeof(USBMIDI_Message_t)] SL_ATTRIBUTE_ALIGN(4);

/*
 * FIFO for received (from USB OUT) messages.
 */
static USBMIDIFIFO_t rxmsgfifo;

/*
 * FIFO for outgoing (to USB IN) messages.
 */
static USBMIDIFIFO_t txmsgfifo;

/**
 * Initialize things that need to be initialized.
 * As of now, it's just the receive message FIFO.
 */
void USBMIDI_Init(void)
{
	USBMIDIFIFO_Init(&rxmsgfifo);
	USBMIDIFIFO_Init(&txmsgfifo);
} // USBMIDI_Init(void)

/**
 * Check the receive (USB MIDI OUT) FIFO to see if any messages are waiting.
 * If so, the message will be in msg and the function returns true.
 * (This function hides the static rxmsgfifo from the larger program.)
 */
bool USBMIDI_RxFifoPop(USBMIDI_Message_t *msg)
{
	return USBMIDIFIFO_Pop(&rxmsgfifo, msg);
} // USBMIDI_RxFifoPop()

/**
 * This callback is invoked when the endpoint assigned to the USB MIDI Out function
 * has received data. It should have one or more four-byte USB MIDI data packets.
 * The data are in OutEpBuf, which is a packet buffer passed in the call to USBD_Read().
 * Take each message packet and push it onto our packet FIFO.
 */
static int USBMIDI_RxXferCompleteCb(USB_Status_TypeDef status, uint32_t xferred, uint32_t remaining)
{
	USBMIDI_Message_t *epb;		// so we don't destroy our endpoint packet buffer
	USBMIDI_Message_t mep;		// next packet in our endpoint buffer

	if( status == USB_STATUS_OK )
	{
		epb = OutEpBuf;
		while( xferred )
		{
			mep = *epb++;
			USBMIDIFIFO_Push(&rxmsgfifo, &mep);
			xferred -= sizeof(mep);
		}
	}

	return USB_STATUS_OK;
} // USBMIDI_RxXferCompleteCb()

/**
 * This callback is invoked when the endpoint assigned to the USB MIDI In function has finished
 * sending a packet.
 * It may also be called by USBMIDI_Write() after a message was pushed to the transmit FIFO. In that
 * case, we only call this if the endpoint is not busy.
 * In either case, we don't care about xferred or remaining. We will look to see if there is
 * anything in the transmit FIFO, in which case we pop it and start the write.
 */
static int USBMIDI_TxXferCompleteCb(USB_Status_TypeDef status, uint32_t xferred, uint32_t remaining)
{
	USBMIDI_Message_t *epb;		// so we don't destroy our endpoint packet buffer
	USBMIDI_Message_t mep;		// next packet to write to our endpoint buffer
	uint32_t bytecnt;			// bytes loaded into transmit buffer.

	if( status == USB_STATUS_OK )
	{
		bytecnt = 0;
		epb = InEpBuf;

		while( USBMIDIFIFO_Pop(&txmsgfifo, &mep) )
		{
			*epb++ = mep;
			bytecnt += sizeof(mep);
		}

		if( bytecnt )
		{
			USBD_Write(MS_EP_DATA_IN, InEpBuf, bytecnt, USBMIDI_TxXferCompleteCb);
		}
	}

	return USB_STATUS_OK;
} // USBMIDI_TxXferCompleteCb()

/**
 * Start a read (OUT) transfer on our MIDI Streaming endpoint
 * if the endpoint is not already busy.
 */
void USBMIDI_Read(void)
{
	if( !USBD_EpIsBusy( MS_EP_DATA_OUT ))
	{
		USBD_Read(MS_EP_DATA_OUT, OutEpBuf, USB_FS_BULK_EP_MAXSIZE, USBMIDI_RxXferCompleteCb);
	}
} // USBMIDI_Read()

/**
 * Write a message to the outgoing (USB IN) FIFO.
 * Then if the endpoint is not busy, call the callback to kick off transmissions.
 * (The callback will pop the FIFO and send the messages with USBD_Write() ).
 */
void USBMIDI_Write(USBMIDI_Message_t *msg)
{
	USBMIDIFIFO_Push(&txmsgfifo, msg);

	if( !USBD_EpIsBusy(MS_EP_DATA_IN) )
		USBMIDI_TxXferCompleteCb(USB_STATUS_OK, 0, 0);
} // USBMIDI_Write()



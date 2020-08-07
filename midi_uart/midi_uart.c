/**
 * midi_uart.c
 *
 *  Created on: Jan 19, 2020
 *      Author: andy
 *
 * A serial-MIDI interface for Tiva-M4C1294 processors, based on earlier work
 * that targeted EFM8 micros.
 *
 * This includes a message parser for incoming messages.
 * This includes a transmit message FIFO and necessary handler functions.
 *
 * The initializer sets up the UART for MIDI operation, 31.25 kbps.
 *
 * The two main interfaces exposed here are methods to read a MIDI message
 * that was received on the IN port and to write a MIDI message to the OUT port.
 *
 ***
 *
 * *** Writing messages to the MIDI OUT port   ***
 *
 * The MIDI OUT port is serviced by the serial transmitter.
 * An event that needs to send a MIDI message out the serial port should simply
 * write that message to the transmit-message FIFO.
 *
 * *** Reading messages from the MIDI IN port ***
 *
 * On a periodic basis, the function called MIDIUART_readMessage() should be called.
 * When that function returns true, the argument msg will contain a new four-byte
 * USB-MIDI message packet.
 */
#include <stdint.h>
#include <stdbool.h>
#include "midi.h"
#include "midi_uart.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/uart.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "inc/hw_types.h"
#include "inc/hw_nvic.h"
#include "pconfig.h"

#include "usb_midi.h"
#include "midi_uart.h"

/**
 * Set up the serial port for MIDI operation.
 * This populates the port structure with the necessary details.
 * Set up the serial port for MIDI operation.
 * This populates the port structure with the necessary details.
 * @param port is the port structure that should be passed to all functions.
 * @param uartbase is the base address of the UART's register space
 * @param scperiph is the corresponding peripheral number passed to SysCtl functions
 * @param sysclkfreq is the clock frequency as set by SysCtlClockFreqSet().
 * @param cablenum is the USB "cable number" we use to distinguish this port from the USB perspective
 *
 * The pins are configured (for now) in PinoutSet() in pinout.c.
 */
void MIDIUART_Init(midiport_t *port, uint32_t uartbase, uint32_t scperiph, uint32_t sysclkfreq, uint8_t cablenum, uint32_t intnum)
{
    /*
     * Initialize the port structure.
     */
    port->uartbase = uartbase;
    port->uartint = intnum;
    port->cablenum = cablenum;

    port->cin = 0;
    port->bytecnt = 0;
    port->bytesinpacket = 0;
    port->rxstate = MU_IDLE;
    port->txfifohead = 0;
    port->txfifotail = 0;
    port->txidle = 1;           // start in mode where we are not transmitting.

    /*
     * Set up the port hardware.
     *
     * First, enable the peripheral itself.
     */
    MAP_SysCtlPeripheralEnable(scperiph);
    while (!MAP_SysCtlPeripheralReady(scperiph))
        ;
    /*
     * Configure the UART for 31,250 bps, 8-N-1 operation.
     */
    MAP_UARTConfigSetExpClk(uartbase, sysclkfreq, 31250,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                             UART_CONFIG_PAR_NONE));

    // No transmit FIFO, so interrupt at end of byte transmission.
    MAP_UARTTxIntModeSet(uartbase, UART_TXINT_MODE_EOT);

    /*
     * Specify which interrupts will be used, and enable them.
     * We only care about the transmit interrupt.
     */
    MAP_UARTIntEnable(uartbase, UART_INT_TX);
    MAP_IntEnable(intnum);
    // enable pullup.
    MAP_GPIOPadConfigSet(GPIO_PORTC_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    /*
     * Enable the UART.
     */
    ROM_UARTEnable(uartbase);
}

/**
 * Write the given message to the MIDI OUT message FIFO.
 *
 * @param[in]  port     Pointer to the structure which holds this port's data.
 * @param[in]  msg      Pointer to an array of bytes that comprise a MIDI message.
 * @param[in]  msize    The number of bytes in that message.
 *
 * This function will block if there is no room in the FIFO for the message.
 *
 * After pushing a byte to the message FIFO, we check to see if the serial transmitter
 * is idle (not sending anything). If so, then we force a software trigger for the
 * serial port. The ISR will then check to see if there's anything in the message
 * FIFO, and since there will be, it'll send that along.
 *
 * The serial transmitter's ISR is the only place that pops the message FIFO.
 *
 * We capture the current state of the head and tail pointers for use in the
 * comparisons to ensure that they don't change in the middle of that comparison.
 */
void MIDIUART_writeMessage(midiport_t *port, uint8_t *msg, uint8_t msize)
{
    bool bIntStatus;
    uint8_t thishead;
    uint8_t thistail;

    while( msize > 0 )
    {
        thishead = port->txfifohead;
        thistail = port->txfifotail;

        // Check to see if there is room in the FIFO.
        if( ((thishead + 1) % MIDI_TX_FIFO_SIZE) != thistail )
        {
            // Yes, there is room.
        	// Write the byte to the message FIFO.
        	port->txmsgfifo[port->txfifohead] = *msg++;

        	// bump write pointer.
        	bIntStatus = MAP_IntMasterDisable();
        	port->txfifohead++;
        	if( MIDI_TX_FIFO_SIZE == port->txfifohead )
        	{
        		port->txfifohead = 0;
        	}

        	--msize; // one less byte in this message to send.

        	if( !bIntStatus )
                MAP_IntMasterEnable();

            // if the serial port is idle, kick-start it by tripping its interrupt.
            if( port->txidle )
            {
                // enable unprivileged access to SWTRIG register.
                //HWREG(NVIC_CFG_CTRL) |= NVIC_CFG_CTRL_MAIN_PEND;
                MAP_IntTrigger(port->uartint);
                //HWREG(NVIC_CFG_CTRL) &= ~NVIC_CFG_CTRL_MAIN_PEND;
            }

    	}
    }
}

/**
 * Check to see if there is a new packet in the serial receive FIFO.
 * This is implemented as a state machine.
 *
 * The idea is to read the message and parse it, and stuff the results into a four-byte
 * MIDI USB Message Packet. Since there is only one originating port, the cable
 * number is the constant UART_CN (in the user's midi_config.h).
 *
 * In all cases, when we first get here, if the receive FIFO count is zero, return
 * 0 immediately, indicating nothing to read.
 *
 * We start in the idle state, looking for a status byte.
 * If the count is non-zero, pop the FIFO, and inspect it: is it a status or
 * data byte? If status, determine how many data bytes are associated with the
 * message. This is mostly straightforward, as there are only eight message types.
 *
 * The exception is with SysEx, where any number of bytes may follow the 0xF0 status marker.
 * In this case, read up to four bytes from the FIFO, looking to see if they are the
 * 0xF7 EOX marker. If not, they go into one of the bytes of the MEP.
 *
 * Byte 1 (after the SOX) cannot be 0xF7 as all SysEx messages must have at least
 * one data byte.
 *
 * If byte 2 is EOX, then the port->cin code is set to 0x5, mep.byte1 was the 1st data byte read,
 * and we pad byte2 and byte3 with 0x00.
 *
 * If byte 3 is EOX, then the port->cin code is set to 0x06, mep.byte1 was the first data byte read,
 * byte2 is the second, and we pad byte3 with 0x00.
 *
 * If byte4 is EOX, then the port->cin code is set to 0x07, the mep.byte1, 2, 3 are what was read,
 * and that's the end of our message.
 *
 * If byte4 is not EOX (it's the next data byte), save it for the next time this function is
 * called, and set the port->cin code to 0x04.
 *
 * After all of that, set the return value to something non-zero and return. The caller will
 * likely just write the new packet to the USB port.
 *
 * The port->cin code is a static variable, so the next time readMessage is called, if it is
 * 0x04 (SysEx starts or continues), we know that we are continuing the handling of a SysEx
 * message.
 *
 *******************************************************************************
 * STATE TABLE.
 *
 * Call the function with a pointer to the USB MIDI message packet buffer, which
 * is four bytes.
 *
 * Start in idle, check count, if zero, return, else pop FIFO. This byte should
 * be STATUS. Parse the status byte and determine how many data bytes are to follow,
 * and what the actual event (CIN and cable number) part of the packet is.
 * Set byte 1 of the packet to this status value.
 * Clear bytes 2 and 3 of the packet, as we may or may not get data for them.
 * Set the byte count and set the state to waiting for byte2.
 *
 * In waiting for data, check rx fifo byte count. If it is zero, exit. The next
 * time this is called it'll jump to this state.
 *
 * If bytes are waiting, read the next byte. Save it in byte2. If this is the
 * last byte of the packet, return true so the caller knows it has a packet and
 * set the state to idle. If not, set the set to waiting for byte3. Check the
 * FIFO count again, if zero, exit, and the next time this is called we jump to
 * the waiting for byte3 state. If there is a byte, read it and save it in byte3.
 * Return true so caller can handle the whole packet. Otherwise return false and
 * deal the next time.
 *
 * If the STATUS byte is SOX, we have to parse differently. That's because the
 * amount of data coming from the sender can be anything, but we have to set the
 * proper code index number for each MIDI USB packet. We need to read at least
 * four bytes from the FIFO to know which CIN to use.
 *
 * So when we see SOX, set the state to SOX1. Check to see if more bytes are available.
 * If not, exit in that state. If so, set the state to SOX1 and read it and save
 * it as byte1. Check count.
 *
 *******************************************************************************/

/**
 * Build a complete MIDI message in the USB-MIDI packet format from bytes received
 * from the serial MIDI IN port. The USB-MIDI packet format is chosen for convenience
 * and in ease of parsing.
 *
 * This method should be called on a periodic basis. Bytes received from the serial
 * port go into the port's FIFO. This ensures that we don't lose message bytes while
 * we are busy doing other things.
 *
 * When this method is called, the port->rxstate of that FIFO is checked, and if one or
 * more bytes are waiting, we pop them and build a message packet. After an entire
 * packet has been received, the function returns true.
 *
 * This function will not return until there are no more bytes in the serial receiver
 * FIFO. It may require multiple calls to this function to complete a message.
 *
 * To support more than one serial MIDI port, we have a parameter midiport which
 * is a pointer to a structure that has info necessary to distinguish this port
 * from another. All of the port status and port->rxstate machine info are in that structure.
 *
 * @param[in] uartbase  The base address of the UART peripheral used for MIDI.
 * @param[in,out] msg   The assembled USB-MIDI message packet is returned here.
 * @return True when msg contains an entire USB-MIDI message packet.
 */
bool MIDIUART_readMessage(midiport_t *port, USBMIDI_Message_t *msg)
{
    // these are newly assigned at every call.
    bool done;                                  //!< indicates packet finished or not, the return value
    uint8_t newbyte;                            //!< This was read from the FIFO

    // start not done, obviously. This will be set as necessary.
    done = false;

    // now stay here until we've read and handled an entire packet, or we've
    // emptied the receive FIFO and we have to wait for more bytes.

    while (!done && MAP_UARTCharsAvail(port->uartbase))
    {
        // Get the next byte in the FIFO. The port->rxstate decoder will decide what it is
        // and what to do with it.
        newbyte = MAP_UARTCharGet(port->uartbase);

        switch (port->rxstate) {
            case MU_IDLE :
                // clear byte2 and byte3 here, on the chance that this newest message
                // will not need them.
                msg->byte2 = 0x00;
                msg->byte3 = 0x00;

                // Is it a SOX byte?
                if (newbyte == MIDI_MSG_SOX) {
                    // SYSEX messages require at least one data byte before EOX, so
                    // we must fetch it. At this point we don't know which port->cin to use.
                    // But we do know that byte1 is the SOX byte.
                    msg->byte1 = MIDI_MSG_SOX;
                    port->rxstate = MU_SYSEX1;

                } else if (newbyte < 0x80) {
                    // running status now active. Use the previous port->cin/CN and
                    // byte1 (the previous status). The byte we just read is
                    // byte2 of the packet.
                    msg->byte2 = newbyte;

                    // See if we need one more byte to complete the packet.
                    // If so, wait for it, otherwise this packet is done.
                    if (port->bytesinpacket == 2) {
                        // yes, one more data byte to fetch:
                        port->rxstate = MU_DATABYTE3;
                    } else {
                        // no more for this packet, send it. Note we cleared byte
                        // at entry to this port->rxstate.
                        port->rxstate = MU_IDLE;
                        done = true;
                    } // bytes in packet

                } else {

                    // not SOX, but it is some kind of status.
                    // What is it? This will let us fill in the Code Index Number
                    // as well as determining how many data bytes will follow.
                    // Make sure to mask off the channel number.
                    // The "single byte" message cannot originate from the UART, as far
                    // as I can tell.

                    switch (newbyte & 0xF0) {
                    case MIDI_MSG_NOTEOFF :
                        port->cin = USB_MIDI_CIN_NOTEOFF;
                        port->bytesinpacket = 2;
                        break;
                    case MIDI_MSG_NOTEON :
                        port->cin = USB_MIDI_CIN_NOTEON;
                        port->bytesinpacket = 2;
                        break;
                    case MIDI_MSG_POLYPRESSURE :
                        port->cin = USB_MIDI_CIN_POLYKEYPRESS;
                        port->bytesinpacket = 2;
                        break;
                    case MIDI_MSG_CTRLCHANGE :
                        port->cin = USB_MIDI_CIN_CTRLCHANGE;
                        port->bytesinpacket = 2;
                        break;
                    case MIDI_MSG_PROGCHANGE :
                        port->cin = USB_MIDI_CIN_PROGCHANGE;
                        port->bytesinpacket = 1;
                        break;
                    case MIDI_MSG_CHANNELPRESSURE :
                        port->cin = USB_MIDI_CIN_CHANPRESSURE;
                        port->bytesinpacket = 1;
                        break;
                    case MIDI_MSG_PITCHBEND :
                        port->cin = USB_MIDI_CIN_PITCHBEND;
                        port->bytesinpacket = 2;
                        break;
                    case MIDI_MSG_SOX :
                        // this is cheating, because I don't handle SOX here, but I need
                        // to handle the other System Common messages.
                        switch (newbyte) {
                        case  MIDI_MSG_MTCQF :
                        case  MIDI_MSG_SS :
                            // two-byte System Common (one data byte)
                            port->cin = USB_MIDI_CIN_SYSCOM2;
                            port->bytesinpacket = 1;
                            break;
                        case MIDI_MSG_SPP :
                            // three byte System Common (two data bytes)
                            port->cin = USB_MIDI_CIN_SYSCOM3;
                            port->bytesinpacket = 2;
                            break;
                        case MIDI_MSG_F4 :
                        case MIDI_MSG_F5 :
                        case MIDI_MSG_TUNEREQ :
                            // one byte System Common (no data byte)
                            port->cin = USB_MIDI_CIN_SYSEND1;
                            port->bytesinpacket = 0;
                            break;
                        default :
                            // the rest are Real Time messages with no data.
                            port->cin = USB_MIDI_CIN_SINGLEBYTE;
                            port->bytesinpacket = 0;
                            break;
                        } // inner switch (if MIDI_MSG_SOX)
                        break;
                    default:
                        // not possible.
                        break;
                    } // switch (newbyte & 0xF0)

                    // now we know how many bytes we need to fetch from the FIFO
                    // to finish up this packet, so set the next port->rxstate properly.
                    if (0 == port->bytesinpacket) {
                        // we do not need to fetch any more bytes for this packet.
                        done = true;
                        port->rxstate = MU_IDLE;
                    } else {
                        // we need to fill at least mep->byte2 and possibly byte3
                        port->bytecnt = port->bytesinpacket;
                        port->rxstate = MU_DATABYTE2;
                    }

                    // The status byte is byte 1 of our packet.
                    msg->byte1 = newbyte;

                    // and we know the event header byte from the Code Index Number
                    // we set above.
                    msg->header = USB_MIDI_HEADER(port->cablenum, port->cin);

                } // if newbyte
                break; // out of idle.

            case MU_DATABYTE2 :
                // the next thing in the FIFO is byte2 of the midi event packet.
                msg->byte2 = newbyte;
                --(port->bytecnt);

                // if there is one more byte in this packet, we have to read it,
                // otherwise we are done.
                if (port->bytecnt) {
                    port->rxstate = MU_DATABYTE3;
                } else {
                    port->rxstate = MU_IDLE;
                    done = true;
                }
                break;

            case MU_DATABYTE3 :
                // get the last byte of the packet from the serial receive FIFO,
                // and we are done. we no longer care about bytecnt.
                msg->byte3 = newbyte;
                done = true;
                port->rxstate = MU_IDLE;
                break;

            case MU_SYSEX1 :
                // we are here because we got a SOX byte. There must be at least
                // one data byte in a SYSEX packet, so read it from the FIFO.
                msg->byte2 = newbyte;

                // if this byte is EOX, then this is that special two-byte SysEx
                // packet, which means we are done. it also means we know which
                // port->cin to assign.
                if (msg->byte2 == MIDI_MSG_EOX) {
                    msg->header = USB_MIDI_HEADER(port->cablenum, USB_MIDI_CIN_SYSEND2);
                    done = true;
                    port->rxstate = MU_IDLE;
                } else {
                    // there is at least one more data byte, so go fetch it.
                    port->rxstate = MU_SYSEX2;
                }
                break;

            case MU_SYSEX2 :
                // we are here because we are in a SysEx packet and there is another
                // byte for it. This will fill the MIDI packet byte 3.
                msg->byte3 = newbyte;

                // if this byte is EOX, then we have the special three-byte SysEx
                // packet, which means we are done. It also means we know which port->cin
                // to assign.
                if (msg->byte3 == MIDI_MSG_EOX) {
                    msg->header = USB_MIDI_HEADER(port->cablenum, USB_MIDI_CIN_SYSEND3);
                    port->rxstate = MU_IDLE;
                } else {
                    // it's not the end of the packet. There is more. But we have
                    // completely filled our MIDI packet, so send it off.
                    msg->header = USB_MIDI_HEADER(port->cablenum, USB_MIDI_CIN_SYSEXSTART);
                    // but we know that the next byte in the serial receive FIFO is
                    // part of the SysEx message, so go get it.
                    port->rxstate = MU_SYSEX1;
                }

                // in any case if we are in this port->rxstate we've filled an entire packet
                // so send it.
                done = true;

                break;

            default :
                // never get here.
                break;
        } // end of port->rxstate register switch
    } // end of while (we've got something to read and process

    // tell caller if we have a complete packet or not.
    return done;
}

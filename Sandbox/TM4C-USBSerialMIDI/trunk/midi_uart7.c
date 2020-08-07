/*
 * midi_uart7.c
 *
 *  Created on: Jan 27, 2020
 *      Author: apeters
 *
 * Most of the UART MIDI code can be abstracted and shared so we can support
 * more than one serial MIDI port in a design. Each function has an argument
 * of a pointer to a structure which holds all relevant information about both
 * the UART itself and the software FIFO used to manage messages.
 *
 * However, each UART has its own interrupt vector. Since we can't "call" an ISR
 * and include the pointer to that structure, that structure has to be global.
 *
 * In this source, which needs to be created for each UART that is a used for MIDI,
 * we instantiate the structure for this UART. The ISR for this UART is here, too.
 *
 * The base address of the specific UART used for this port must be defined in pconfig.h.
 */
#include <stdint.h>
#include <stdbool.h>
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/uart.h"
#include "driverlib/interrupt.h"
#include "pconfig.h"
#include "midi_uart.h"

/**
 * Declare an instance of the midiport_t structure used for this port.
 * It will be initialized in the call to MIDIUART_Init().
 */
volatile midiport_t mpuart7;

/**
 * ISR for the UART used for MIDI.
 *
 * The UART's FIFOs are disabled.
 *
 * The transmit interrupt is enabled. This ISR is invoked under two conditions:
 *
 * a) By a software trigger. If the transmitter is idle when a new byte is written
 *    to the message FIFO, a software trigger is fired. We should pop the message
 *    FIFO and write that byte to the transmitter. Clear the idle flag so when the
 *    next byte is written to the message FIFO, we won't kick-start this again.
 *
 * b) When the transmitter finishes sending a byte. In this case, the idle flag
 *    should be cleared, and we should check to see if there are more bytes in the
 *    message FIFO. If so, pop one and transmit it.
 *
 * If we determine that there are no more bytes in the message FIFO, set the idle
 * flag so we can force the kick-start with the next message.
 *
 * We do not need any receive interrupts, as we'll poll for new incoming bytes with
 * calls to MIDIUART_readMessage().
 */
void MIDIUART7_IntHandler(void)
{
    uint32_t status;
    bool bIntStatus;

    // should only be the transmit interrupt, which gets asserted when the level in
    // the transmit FIFO drops below the programmed threshold.

    // Clear it:
    status = MAP_UARTIntStatus(MIDI_UART7_BASE, UART_INT_TX);
    MAP_UARTIntClear(MIDI_UART7_BASE, status);

    MAP_IntDisable(MIDI_UART7_INT);

    // If message FIFO is empty, we have nothing more to do.
    // If not, pop it and send the next byte.
    if( mpuart7.txfifohead == mpuart7.txfifotail )
    {
        // nothing more to load into transmitter, so ..
        mpuart7.txidle = 1;

    } else {
        // There is something to transmit. First, disable interrupts so
        // we finish this operation without being annoyed.
        bIntStatus = MAP_IntMasterDisable();

        // so message-fifo write won't try to kick-start.
        mpuart7.txidle = 0; // busy!

        // Pop the message FIFO, send that byte.
        MAP_UARTCharPut(MIDI_UART7_BASE, mpuart7.txmsgfifo[mpuart7.txfifotail]);

        // bump read pointer.
        mpuart7.txfifotail++;
        if(  MIDI_TX_FIFO_SIZE == mpuart7.txfifotail )
            mpuart7.txfifotail = 0;

        // re-enable interrupt.
        if( !bIntStatus)
            MAP_IntMasterEnable();
    }
    MAP_IntEnable(MIDI_UART7_INT);
}





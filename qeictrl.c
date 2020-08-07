/*
 * qei.c
 *
 *  Created on: Jun 24, 2020
 *      Author: andy
 */
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "utils/uartstdio.h"
#include "driverlib/qei.h"

#include "qeictrl.h"
#include "midi.h"  // for message constants.
#include "midi_uart7.h"   // for writing to the serial MIDI port.
#include "pconfig.h"

/**
 * Handler for encoder interrupt.
 * Interrupt asserted when velocity timer expires.
 * Save the velocity in the given register and set a flag.
 */
static volatile uint32_t velocity;
bool velflag;

void QEIntHandler(void)
{
    static uint32_t scopetrigger = 0;

    MAP_QEIIntClear(QEI0_BASE, QEI_INTTIMER | QEI_INTDIR);
    velocity = MAP_QEIVelocityGet(QEI0_BASE);
    velflag = true;

    MAP_GPIOPinWrite(QEI_SCOPE_PORT, QEI_SCOPE_PIN, scopetrigger);
    if( scopetrigger )
        scopetrigger = 0;
    else
        scopetrigger = QEI_SCOPE_PIN;
}

/**
 * Set up the QEI controller.
 */
void QEI_Setup(void)
{
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_QEI0);
    while( !MAP_SysCtlPeripheralReady(SYSCTL_PERIPH_QEI0))
        ;
    MAP_QEIConfigure(QEI0_BASE, (QEI_CONFIG_CAPTURE_A | QEI_CONFIG_CLOCK_DIR), 128 );
    MAP_QEIEnable(QEI0_BASE);
    MAP_QEIVelocityConfigure(QEI0_BASE, QEI_VELDIV_4, 65536);
    MAP_QEIVelocityEnable(QEI0_BASE);
    MAP_QEIIntClear(QEI0_BASE, QEI_INTTIMER | QEI_INTERROR | QEI_INTDIR | QEI_INTINDEX);
    MAP_QEIIntEnable(QEI0_BASE, QEI_INTTIMER | QEI_INTDIR);
    velflag = 0;
}

/**
 * Look for encoder changes and send a MIDI Control Change message.
 *
 * The encoder has a "position." When it rotates clockwise, the "position" increases
 * until it saturates at 127. when it rotates counter-clockwise, the "position"
 * decreases until it saturates at 0.
 *
 * Velocity controls how fast the "position" increments or decrements.
 */
void QEI_Task(void)
{
    uint32_t qei_newpos;                //!< new read from the encoder.
    static uint32_t qei_oldpos = 0;     //!< previously read from encoder for compare
    uint8_t msg[3];                     //!< MIDI message to send.


    if( velflag )
    {
//        UARTprintf("Vel: %u\n", velocity);
        velflag = 0;
    }

    qei_newpos = MAP_QEIPositionGet(QEI0_BASE);
    if( qei_newpos != qei_oldpos)
    {
        // Send MIDI message with new position.
        msg[0] = MIDI_MSG_CTRLCHANGE;
        msg[1] = MIDI_CC_GP8;
        msg[2] = (uint8_t) qei_newpos;
        MIDIUART_writeMessage(&mpuart7, msg, 3);
        // Save for next time through.
        qei_oldpos = qei_newpos;
    }
}




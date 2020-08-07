/*
 * buttons.c
 *
 *  Created on: Jan 20, 2020
 *      Author: andy
 *
 *  Button handler functions.
 *
 *  A button press sends a MIDI Note On message.
 *  A button release sends a MIDI Note Off message.
 *
 *  Sending a message means "push a message onto the outgoing message stack."
 *
 */
#include <stdint.h>
#include <stdbool.h>
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/gpio.h"
#include "pconfig.h"
#include "midi.h"
#include "midi_uart.h"
#include "midi_uart7.h"
#include "buttons.h"

static volatile uint8_t btnstate;

/**
 * Set up buttons as interrupt sources.
 * Both assert an interrupt on either edge.
 */
void Button_Init(void)
{
    MAP_GPIOIntTypeSet(BTN_PORT, BTN_0 | BTN_1, GPIO_BOTH_EDGES);
    MAP_GPIOIntClear(BTN_PORT, BTN_0 | BTN_1);
    MAP_GPIOIntEnable(BTN_PORT, BTN_0 | BTN_1);
    MAP_IntEnable(BTN_INT);
}

/**
 * Button ISR.
 * Since we interrupt on both rising and falling edge, we have to check the state
 * of the button which caused the interrupt.
 * We will default to MIDI channel 0.
 */
void ButtonIntHandler(void)
{
    uint32_t intstatus;		// interrupt status.
    uint8_t pins;			// read from pins

    intstatus = MAP_GPIOIntStatus(BTN_PORT, BTN_0 | BTN_1);
    btnstate = 0;

    if( (intstatus & BTN_0) )
    {
        MAP_GPIOIntClear(BTN_PORT, BTN_0);
        pins = MAP_GPIOPinRead(BTN_PORT, BTN_0);

        // pin high means released (off), low means pressed (on)
        if( pins )
        {
            btnstate |= BTNSTATE_RE0; // rising edge when released

        } else {
            btnstate |= BTNSTATE_FE0;   // falling edge when pressed
        }

    }

    if( (intstatus & BTN_1) )
    {
        MAP_GPIOIntClear(BTN_PORT, BTN_1);
        pins = MAP_GPIOPinRead(BTN_PORT, BTN_1);
        // pin high means released (off), low means pressed (on)
        if( pins )
        {
            btnstate |= BTNSTATE_RE1;
        } else {
            btnstate |= BTNSTATE_FE1;
        }
    }
}

/**
 * Return flags indicating what changed.
 */
uint8_t Button_getState(void)
{
    uint8_t retval;
    retval = btnstate;
    btnstate = 0;
    return retval;
}




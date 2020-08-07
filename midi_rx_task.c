/*
 * midi_rx_task.c
 *
 *  Created on: Jun 24, 2020
 *      Author: andy
 */


#include <stdint.h>
#include <stdbool.h>
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"

#include "midi.h"
#include "midi_uart.h"
#include "midi_uart7.h"
#include "pconfig.h"

#include "usb_midi.h"
#include "midi_rx_task.h"

/**
 * Check for incoming MIDI messages and parse them.
 *
 * For the time being, we handle two control-change messages, and they simply
 * control the two LEDs. We won't bother dealing with cable numbers or the
 * Code Index Number. Just inspect the three bytes of the message packet.
 * Byte 1 is Status. Look for MIDI_MSG_CTRLCHANGE.
 * Byte 2 is the control number. Look for MIDI_CC_GP5 for LED0 and MIDI_CC_GP6 for LED1.
 * Byte 3 is the intensity of the LED. 64 and greater is on, 63 and less is off.
 */
void MIDI_Rx_Task(void)
{
    static USBMIDI_Message_t msg;
    uint8_t led;

    if( MIDIUART_readMessage(&mpuart7, &msg) )
    {
        if( msg.byte1 == MIDI_MSG_CTRLCHANGE )
        {
            switch( msg.byte2 )
            {
            case MIDI_CC_GP5:
                if( msg.byte3 > 63)
                    led = LED_LED0;
                else
                    led = 0;
                MAP_GPIOPinWrite(LED_PORT, LED_LED0, led);
                break;

            case MIDI_CC_GP6:
                if( msg.byte3 > 63)
                    led = LED_LED1;
                else
                    led = 0;
                MAP_GPIOPinWrite(LED_PORT, LED_LED1, led);
                break;
            }
        }
    }
}


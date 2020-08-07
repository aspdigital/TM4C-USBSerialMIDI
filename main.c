/**
 * An implementation of the USB MIDI class, for the TI Tiva TM4C1294 microcontroller.
 *
 * This design has both serial (UART) MIDI and USB MIDI.
 *
 * The USB MIDI has two "cables" or ports in each direction.
 *
 * Cable 0 IN (from the host) will control the LEDs. (Maybe I can do PWM.)
 * Cable 0 OUT (to the host) sends button presses as control changes.
 * Cable 1 IN drives serial port OUT.
 * Cable 1 OUT gets messages from serial port IN and sends them to the host.
 *
 *************
 * UART MIDI:
 *
 * IN.
 * As bytes come in, they are collected and a USB MIDI message is built. The main loop periodically
 * checks MIDISERIAL_readUSBMessage() and when that returns true, it has a new message which is
 * written back to the USB host.
 *
 * OUT.
 * MIDI data from USB are written to a message packet FIFO. The main loop pops that FIFO,
 * examines the message and if it is meant for Cable 1 (the serial port), the message bytes
 * and message size are written to MIDISERIAL_SendMessage()
 *
 *************
 * USB MIDI.
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <usbmidi_types.h>

#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/qei.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "usblib/usblib.h"
#include "usblib/device/usbdevice.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "pconfig.h"
#include "pinout.h"
#include "clcd.h"
#include "midi_uart.h"
#include "midi_uart7.h"
#include "buttons.h"
#include "qeictrl.h"
#include "midi_rx_task.h"
#include "midi_usb_rx_task.h"

#include "usbmidi.h"


//*****************************************************************************
//
// System clock rate in Hz.
//
//*****************************************************************************
uint32_t g_ui32SysClock;
uint32_t g_ui32SysTickCount;

#define SYSTICKS_PER_SECOND 100
#define SYSTICK_PERIOD_MS   (1000 / SYSTICKS_PER_SECOND)

void
SysTickIntHandler(void)
{
    //
    // Update our system tick counter.
    //
    g_ui32SysTickCount++;
}

int main(void)
{
    // uint32_t ui32SysClock;
    uint32_t ui32PLLRate;

    uint8_t btnstate;
    uint8_t msg[3];         	// This message is three bytes
    USBMIDI_Message_t txmsg;	// and here it is as a USB MIDI message
    bool wasConnected = false;

    // The SYSCTL_MOSC_HIGHFREQ parameter is used when the crystal
    // frequency is 10MHz or higher.
    MAP_SysCtlMOSCConfigSet(SYSCTL_MOSC_HIGHFREQ);

    //
    // Run from the PLL at 120 MHz.
    //
    g_ui32SysClock = MAP_SysCtlClockFreqSet(
            (SYSCTL_XTAL_25MHZ |
             SYSCTL_OSC_MAIN |
             SYSCTL_USE_PLL |
             SYSCTL_CFG_VCO_480),
             120000000);

    // Set-up pins.
    PinoutSet();

    //
    // Enable the system tick.
    //
    MAP_SysTickPeriodSet(g_ui32SysClock / SYSTICKS_PER_SECOND);
    MAP_SysTickIntEnable();
    MAP_SysTickEnable();

    // Configure Timer1 as a periodic count down 32-bit timer that toggles a pin.
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    while (!MAP_SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER1))
         ;
    MAP_TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC | TIMER_CFG_A_ACT_TOGGLE);
    MAP_TimerLoadSet(TIMER1_BASE, TIMER_A, 1000);
    MAP_TimerEnable(TIMER1_BASE, TIMER_A);

    // TEST
    MAP_GPIOPinWrite(QEI_SCOPE_PORT, QEI_SCOPE_PIN, QEI_SCOPE_PIN);
    MAP_GPIOPinWrite(QEI_SCOPE_PORT, QEI_SCOPE_PIN, 0);
    MAP_GPIOPinWrite(QEI_SCOPE_PORT, QEI_SCOPE_PIN, QEI_SCOPE_PIN);
    MAP_GPIOPinWrite(QEI_SCOPE_PORT, QEI_SCOPE_PIN, 0);

    /**
     * Set up quadrature encoder.
     */
    QEI_Setup();

    /**
     * Set up MIDI UART.
     */
    MIDIUART_Init(&mpuart7, MIDI_UART7_BASE, MIDI_UART7_SYSCTL_PERIPH, g_ui32SysClock, MIDI_UART7_CN, MIDI_UART7_INT);

    /**
     * Set up the buttons.
     */
    Button_Init();

    /**
     * Tell the USB library the CPU clock and the PLL frequency.  This is a
     * new requirement for TM4C129 devices.
     */
    SysCtlVCOGet(SYSCTL_XTAL_25MHZ, &ui32PLLRate);
    USBDCDFeatureSet(0, USBLIB_FEATURE_CPUCLK, &g_ui32SysClock);
    USBDCDFeatureSet(0, USBLIB_FEATURE_USBPLL, &ui32PLLRate);

    /*
     * Initialize the USB stack for device mode.
     * Forcing device mode so that the VBUS and ID pins are not used or
     * monitored by the USB controller.
     */
    USBStackModeSet(0, eUSBModeDevice, 0);
//    USBStackModeSet(0, eUSBModeForceDevice, 0);

    MAP_GPIOPinWrite(LED_PORT, LED_LED0, 0);
#if 0
    HWREG(GPIO_PORTD_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
    HWREG(GPIO_PORTD_BASE + GPIO_O_CR) = 0xff;
    MAP_GPIOPinConfigure(GPIO_PD6_USB0EPEN);
    MAP_GPIOPinTypeUSBAnalog(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    MAP_GPIOPinTypeUSBDigital(GPIO_PORTD_BASE, GPIO_PIN_6);
    MAP_GPIOPinTypeUSBAnalog(GPIO_PORTL_BASE, GPIO_PIN_6 | GPIO_PIN_7);
    MAP_GPIOPinTypeGPIOInput(GPIO_PORTQ_BASE, GPIO_PIN_4);
#endif
    USBMIDI_Init(0);

    //
    // Enable processor interrupts.
    //
    MAP_IntEnable(INT_QEI0);
    MAP_IntMasterEnable();

    //
    // Initialize the UART for console I/O.
    //
    UARTStdioConfig(0, 115200, g_ui32SysClock);

    UARTprintf("Hello, world!\nClock frequency is %u\n", g_ui32SysClock);

    // Enable LCD
    LcdInit();
    LcdMoveCursor(1, 0);
    LcdWriteString("Hello! ");
    LcdWriteChar(0xAF);
    LcdMoveCursor(0, 0);

    /*
     * Forever
     */
    while (1)
    {
    	/*
    	 * Report change in USB device connection status.
    	 */
    	if( USBMIDI_IsConnected() ) {
    		if( wasConnected == false ) {
    			UARTprintf("Connected to bus!\n");
    			wasConnected = true;
    		}
    	} else {
    		if( wasConnected == true ) {
    			UARTprintf("Disconnected from bus!\n");
    			wasConnected = false;
    		}
    	}
        /*
         *  Handle encoder.
         */
        QEI_Task();

        /*
         * Check buttons, and perhaps send a message
         */
        btnstate = Button_getState();

        if( btnstate & BTNSTATE_RE0 )
        {
            // rising edge, so note off.
            msg[0] = MIDI_MSG_NOTEON;
            msg[1] = 0x60;    // middle C
            msg[2] = 0x00;    // off velocity
            MIDIUART_writeMessage(&mpuart7, msg, 3);
            txmsg.header = USB_MIDI_HEADER(1, USB_MIDI_CIN_NOTEOFF );
            txmsg.byte1 = msg[0];
            txmsg.byte2 = msg[1];
            txmsg.byte3 = msg[2];
            USBMIDI_InEpMsgWrite(&txmsg);
        }

        if( btnstate & BTNSTATE_FE0 )
        {
            // falling edge, so note on.
            msg[0] = MIDI_MSG_NOTEON;
            msg[1] = 0x60;    // middle C
            msg[2] = 0x40;    // on velocity
            MIDIUART_writeMessage(&mpuart7, msg, 3);
            txmsg.header = USB_MIDI_HEADER(1, USB_MIDI_CIN_NOTEON );
            txmsg.byte1 = msg[0];
            txmsg.byte2 = msg[1];
            txmsg.byte3 = msg[2];
            USBMIDI_InEpMsgWrite(&txmsg);
        }

        if( btnstate & BTNSTATE_RE1 )
        {
             // rising edge, so note off.
             msg[0] = MIDI_MSG_NOTEON;
             msg[1] = 0x44;    // some note!
             msg[2] = 0x00;    // off velocity
             MIDIUART_writeMessage(&mpuart7, msg, 3);
             txmsg.header = USB_MIDI_HEADER(1, USB_MIDI_CIN_NOTEOFF );
             txmsg.byte1 = msg[0];
             txmsg.byte2 = msg[1];
             txmsg.byte3 = msg[2];
             USBMIDI_InEpMsgWrite(&txmsg);
         }

         if( btnstate & BTNSTATE_FE1 )
         {
             // falling edge, so note on.
             msg[0] = MIDI_MSG_NOTEON;
             msg[1] = 0x44;    // some note
             msg[2] = 0x40;    // on velocity
             MIDIUART_writeMessage(&mpuart7, msg, 3);
             txmsg.header = USB_MIDI_HEADER(1, USB_MIDI_CIN_NOTEON );
             txmsg.byte1 = msg[0];
             txmsg.byte2 = msg[1];
             txmsg.byte3 = msg[2];
             USBMIDI_InEpMsgWrite(&txmsg);
         }

        /*
         * Check for incoming serial MIDI messages and handle them.
         * For right now, this just sets the state of the LEDs.
         */
        MIDI_Rx_Task();

        /*
         * Check for incoming USB MIDI messages.
         * Convert them to ASCII and transmit out the debug UART.
         */
        MIDI_USB_Rx_Task();


    }
}

/*
 * pconfig.h
 *
 * Define pins and peripherals for application-specific use.
 *
 *  Created on: Jan 10, 2020
 *      Author: andy
 */

#ifndef PCONFIG_H_
#define PCONFIG_H_

#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/uart.h"

/**
 * Character LCD interface is all on Port M.
 * We use the four-bit interface.
 */
#define CLCD_PORT GPIO_PORTM_BASE
#define CLCD_D4   GPIO_PIN_0
#define CLCD_D5   GPIO_PIN_1
#define CLCD_D6   GPIO_PIN_2
#define CLCD_D7   GPIO_PIN_3
#define CLCD_RS   GPIO_PIN_4
#define CLCD_E    GPIO_PIN_5
#define CLCD_RW   GPIO_PIN_6
#define CLCD_BL   GPIO_PIN_7

#define CLCD_DATA (CLCD_D7 | CLCD_D6 | CLCD_D5 | CLCD_D4)

/**
 * LEDs on the dev kit.
 */
#define LED_PORT GPIO_PORTN_BASE
#define LED_LED0 GPIO_PIN_0
#define LED_LED1 GPIO_PIN_1
// "Ethernet" LEDs are on PF4 and PF0
#define LED_ENPORT GPIO_PORTF_BASE
#define LED_LED2 GPIO_PIN_0
#define LED_LED3 GPIO_PIN_4

/**
 * Pushbuttons on the dev kit.
 */
#define BTN_PORT GPIO_PORTJ_BASE
#define BTN_0    GPIO_PIN_0
#define BTN_1    GPIO_PIN_1
#define BTN_INT  INT_GPIOJ

/**
 * A scope toggle bit when the encoder ISR is called.
 */
#define QEI_SCOPE_PORT GPIO_PORTL_BASE
#define QEI_SCOPE_PIN GPIO_PIN_3

/**
 * Support for MIDI over UART7.
 */
#define MIDI_UART7_BASE UART7_BASE
#define MIDI_UART7_SYSCTL_PERIPH SYSCTL_PERIPH_UART7
#define MIDI_UART7_INT INT_UART7
#define MIDI_UART7_CN 0


#endif /* PCONFIG_H_ */

/*
 * buttons.h
 *
 *  Created on: Jan 20, 2020
 *      Author: andy
 *
 * Handlers for the two buttons on the dev kit.
 */

#ifndef BUTTONS_H_
#define BUTTONS_H_

#include <stdint.h>
/**
 * Set up buttons as interrupt sources.
 * Both assert an interrupt on either edge.
 */
void Button_Init(void);

/**
 * Get button status.
 * Flags indicate which state changed.
 */
#define BTNSTATE_RE0 0x01
#define BTNSTATE_FE0 0x02
#define BTNSTATE_RE1 0x04
#define BTNSTATE_FE1 0x08

uint8_t Button_getState(void);



#endif /* BUTTONS_H_ */

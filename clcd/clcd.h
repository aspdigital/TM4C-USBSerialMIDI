/*
 * clcd.h
 *
 * Driver for standard character LCDs.
 *
 *  Created on: Jan 10, 2020
 *      Author: andy
 */

#ifndef CLCD_CLCD_H_
#define CLCD_CLCD_H_

#include <stdint.h>
#include <stdbool.h>

void LcdWriteChar(uint8_t dval);
void LcdWriteString(uint8_t *str);
void LcdMoveCursor(uint8_t row, uint8_t col);
void LcdInit(void);
void LcdClear(void);
void LcdClearLine(uint8_t line);
void LcdWriteCGRAM(uint8_t addr, uint8_t pattern);
void LcdCursorBlink(bool blink);

#endif /* CLCD_CLCD_H_ */

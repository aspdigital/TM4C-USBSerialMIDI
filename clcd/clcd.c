/*
 * clcd.c
 *
 * driver for standard character LCDs.
 *
 *  Created on: Jan 10, 2020
 *      Author: andy
 *  Mods:
 *  2020-01-11 andy. Use a standard timer to pace LCD operations.
 *  2020-01-15 andy. Do not use a timer.
 *  2020-01-16 andy. Tested, working as expected.
 *
 *****
 *
 * This driver sets up and uses a standard character LCD in the four bit mode.
 */

#include <stdint.h>
#include <stdbool.h>
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "pconfig.h"

/*
 * LCD commands.
 */
// Clear display:
#define LCD_CLEAR       0x01

// return cursor and LCD to home position:
#define LCD_HOME        0x02

// entry mode set
#define LCD_ENTRYMODE   0x04
// shift display when byte written to display
#define LCD_ENTRYMODE_SHIFTDISPLAY   0x01
// increment the cursor after each byte written:
#define LCD_ENTRYMODE_MOVERIGHT      0x02

// Enable display/cursor:
#define LCD_DISPEN        0x08
// turn cursor blink on:
#define LCD_DISPEN_BLINK     0x01
// turn cursor on:
#define LCD_DISPEN_CURSOR    0x02
// turn display on:
#define LCD_DISPEN_DISPON    0x04

// move cursor/shift display.
#define LCD_MCSD         0x10
// This shifts the existing display
// direction of shift (right if set):
#define LCD_MCSD_RL          0x04
// turn on display shift:
#define LCD_MCSD_SC          0x08

// Function set.
#define LCD_FNSET        0x20
// set character font 5x10 (1) or 5x7 (0)
#define LCD_FNSET_F          0x04
// set # of display lines 1 if 0, 2 if 1:
#define LCD_FNSET_N          0x08
// set interface length 8 bits if 1, 4 bits if 0
#define LCD_FNSET_DL         0x10

// move cursor into CGRAM:
#define LCD_SETCGRAMADDR     0x40

// Move cursor to display:
#define LCD_SETDDRAMADDR     0x80

/**
 * Some constants to define delays.
 * Based on 120 MHz clock.
 */
#define DELAY_37us        622
#define DELAY_43us        800
#define DELAY_160us      2448
#define DELAY_1p52ms    25552
#define DELAY_5ms      127760

/**
 * Delay before we can monitor busy flag.
 */
static void Delay(uint32_t dtime)
{
    volatile uint32_t dxx;
    // Load the timer and start it running.
    dxx = dtime;
    while (dxx--)
        ;
}

/**
 * Simple delay function.
 * Set LCD data pins to inputs.
 * Poll for BUSY flag to be asserted, then again wait for it to go away.
 */
#if 0
static void Delay(void)
{
    uint32_t busy;

    // Set data pins as inputs.
    MAP_GPIOPinTypeGPIOInput(CLCD_PORT, CLCD_DATA );
    // Set RS and RW for data read
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_RS | CLCD_RW, CLCD_RS | CLCD_RW);

    busy = 0;
    while (!busy)
    {
        // Assert E long enough for output to go valid
        MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, CLCD_E);
        MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, CLCD_E);
        // Busy is on D7, read it.
        busy = MAP_GPIOPinRead(CLCD_PORT, CLCD_D7);
        // and turn off E.
        MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0);
        MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0);
    }

    // now wait for busy to go away.
    while (busy)
    {
        // Assert E long enough for output to go valid
        MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, CLCD_E);
        MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, CLCD_E);
        // Busy is on D7, read it.
        busy = MAP_GPIOPinRead(CLCD_PORT, CLCD_D7);
        // and turn off E.
        MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0);
        MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0);
    }
    // Make the data pins outputs again.
    MAP_GPIOPinTypeGPIOOutput(CLCD_PORT, CLCD_DATA );

}  // Delay()
#endif

/**
 * Write a character to the display at the current cursor position.
 * In four-bit mode we write the MS nybble first.
 * On TM4C1294 I measured 320 ns between successive GPIOPinWrite() calls with nothing in between.
 *
 * ST7066U timing is
 *    tAS:  0 ns     time from RS and RW to E rising
 *    tPW:  460 ns   E pulse width
 *    tDSW: 80 ns    data set-up to E falling
 *
 * So for our purposes, we can assert data, RS, RW and E at the same time.
 * To ensure we meed E width, do that twice.
 * Then do everything but E for the third GPIOWrite.
 *
 * There is a requirement of 1200 ns E cycle time. So between nybble access
 * clear E twice.
 *
 * Finally, 37 us are required for the data write to complete.
 */
void LcdWriteChar(uint8_t ch)
{
    uint8_t dval;

    dval = ch >> 4;     // get the upper nybble only
    // scope trigger:
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_BL, CLCD_BL);
    // Write upper nybble. Do this twice to ensure proper E width.
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_DATA | CLCD_RS | CLCD_E | CLCD_RW, dval | CLCD_RS | CLCD_E);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_DATA | CLCD_RS | CLCD_E | CLCD_RW, dval | CLCD_RS | CLCD_E);
    // Clear E, leave everything else asserted. Do this twice to ensure proper cycle time
    // between E rising edge.
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0);

    // write lower nybble
    dval = (0x0f) & ch;
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_DATA | CLCD_RS | CLCD_E | CLCD_RW, dval | CLCD_RS | CLCD_E);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_DATA | CLCD_RS | CLCD_E | CLCD_RW, dval | CLCD_RS | CLCD_E);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0);
    // Wait 43 us.
    Delay(DELAY_43us);
    // scope trigger:
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_BL, 0);
}

/**
 * Write a command to the display.
 * Some commands require longer delays, so allow them.
 * Same timing comments as LcdWriteChar above.
 */
static void LcdWriteCmd(uint8_t cmd, uint32_t dly)
{
    uint8_t dval;

    dval = cmd >> 4; // get the upper nybble

    // scope trigger:
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_BL, CLCD_BL);
    // write the upper nybble. This also turns off RS. Write twice to ensure proper E width
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_DATA | CLCD_RS | CLCD_E | CLCD_RW, dval | CLCD_E);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_DATA | CLCD_RS | CLCD_E | CLCD_RW, dval | CLCD_E);
    // Clear E.
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0x00);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0x00);

    // write lower nybble
    dval = (0x0f) & cmd;
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_DATA | CLCD_RS | CLCD_E | CLCD_RW, dval | CLCD_E);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_DATA | CLCD_RS | CLCD_E | CLCD_RW, dval | CLCD_E);
    // Clear E.
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0x00);

    // finish up with whatever delay is needed
    Delay(dly);
    // scope trigger:
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_BL, 0);
}

/**
 * Write a string of characters to the display, starting at the cursor location.
 */
void LcdWriteString(uint8_t *str)
{
    while (*str != '\0')
    {
        LcdWriteChar(*str);
        ++str;
    }
}

/**
 * Move the cursor to the specified row and column.
 * For a 2-line by 16 display (in 2-line mode)
 * Row 0 starts at address 0x00 and runs to 0x27.
 * Row 1 starts at address 0x40 and runs to 0x67.
 *
 * This addressing should probably be sorted out more rationally.
 */
void LcdMoveCursor(uint8_t row, uint8_t col)
{
    uint8_t ac;

    // Row select > 1 is not allowed.
    if (row > 1)
        return;

    // Column select > 15 is likewise not allowed.
    if (col > 15)
        return;

    ac = LCD_SETDDRAMADDR | col;

    if (row)
        ac |=  0x40;

    LcdWriteCmd(ac, DELAY_37us);
}

/**
 * Initialize everything to do with the LCD. (We should move pinout init here.)
 * Assume we've waited the power-on delay time.
 *
 * We do the 0x30 wakeup command and the 0x20 set-4-bit command as 0x3 and 0x2,
 * respectively, in the PinWrite() functions because CLCD_DATA is four bits and
 * is defined as the lowest 4 bits of the port.
 */
void LcdInit(void)
{
    // Clear the port pins.
    // Note when RS is low we are writing commands.
    MAP_GPIOPinWrite(CLCD_PORT, 0xFF, 0x00);

    // 0x30 command is specific set-up for this display/device, per data sheet
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_DATA, 0x03);
    Delay(DELAY_5ms);
    // #1. Strobe E to load command. Do twice for correct pulse width.
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, CLCD_E);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, CLCD_E);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0x00);
    // Wait 160 us for completion.
    Delay(DELAY_160us);

    // #2. Strobe E to load command. Do twice for correct pulse width.
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, CLCD_E);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, CLCD_E);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0x00);

    // Wait 160 us for completion.
    Delay(DELAY_160us);

    // #3. Strobe E to load command. Do twice for correct pulse width.
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, CLCD_E);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, CLCD_E);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0x00);
    // Wait 160 us for completion.
    Delay(DELAY_160us);

    // Write 0x20 to set 4-bit interface.
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_DATA, 0x02);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, CLCD_E);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, CLCD_E);
    MAP_GPIOPinWrite(CLCD_PORT, CLCD_E, 0x00);
    // Wait 160 us for completion.
    Delay(DELAY_160us);

    // Now we can write proper commands.
    LcdWriteCmd(LCD_FNSET | LCD_FNSET_N, DELAY_37us); // 2 lines, 4-bit interface
    LcdWriteCmd(LCD_DISPEN, DELAY_37us);              // display off, cursor off, no blink
    LcdWriteCmd(LCD_CLEAR, DELAY_1p52ms);             // clear display
    LcdWriteCmd(LCD_HOME, DELAY_1p52ms);              // move cursor home
    LcdWriteCmd(LCD_DISPEN | /* LCD_DISPEN_BLINK | */ LCD_DISPEN_CURSOR | LCD_DISPEN_DISPON, DELAY_37us);
    LcdWriteCmd(LCD_ENTRYMODE | LCD_ENTRYMODE_MOVERIGHT, DELAY_37us);
}

/**
 * Clear the display. Cursor is moved home.
 */
void LcdClear(void)
{
    LcdWriteCmd(LCD_CLEAR, DELAY_1p52ms);
}

/**
 * Clear the selected line.
 */
void LcdClearLine(uint8_t line)
{
    LcdMoveCursor(line, 0);
    LcdWriteString("                ");
    LcdMoveCursor(line, 0);
}

/**
 * Write custom characters to the CGRAM at the specified address.
 */
void LcdWriteCGRAM(uint8_t addr, uint8_t pattern)
{
    // First, access the CGRAM space.
    LcdWriteCmd(LCD_SETCGRAMADDR | addr, DELAY_37us);
    // Now we can write to that location.
    LcdWriteChar(pattern);
}

/**
 * Control whether the cursor should blink or not.
 */
void LcdCursorBlink(bool blink)
{
    uint8_t cmd;

    cmd = LCD_DISPEN | LCD_DISPEN_DISPON;
    if (blink) {
        cmd |= LCD_DISPEN_BLINK;
        cmd |= LCD_DISPEN_CURSOR;
    } else {
        cmd &= ~LCD_DISPEN_BLINK;
        cmd &= ~LCD_DISPEN_CURSOR;
    }
    LcdWriteCmd(cmd, DELAY_37us);
}

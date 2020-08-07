/*
 * midi_rx_task.h
 *
 *  Created on: Jun 24, 2020
 *      Author: andy
 */

#ifndef MIDI_RX_TASK_H_
#define MIDI_RX_TASK_H_

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
void MIDI_Rx_Task(void);

#endif /* MIDI_RX_TASK_H_ */

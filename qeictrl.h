/*
 * qei.h
 *
 *  Created on: Jun 24, 2020
 *      Author: andy
 */

#ifndef QEICTRL_QEICTRL_H_
#define QEICTRL_QEICTRL_H_

/**
 * Look for encoder changes and send a MIDI Control Change message.
 *
 * The encoder has a "position." When it rotates clockwise, the "position" increases
 * until it saturates at 127. when it rotates counter-clockwise, the "position"
 * decreases until it saturates at 0.
 *
 * Velocity controls how fast the "position" increments or decrements.
 */
void QEI_Task(void);

/**
 * Set up the QEI controller.
 */
void QEI_Setup(void);



#endif /* QEICTRL_QEICTRL_H_ */

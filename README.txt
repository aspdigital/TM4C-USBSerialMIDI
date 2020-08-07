SERIAL MIDI example. 

This example shows how the serial MIDI should work. 
We use UART7 for this example. 

** MIDI IN handling **

MIDI messages come in on serial receiver. They are packaged into the four-byte USB-MIDI Event Packet format,
which is easy to parse. The main program loop should call MIDIUART_readMessage() periodically to get a new
message. Perhaps that can be put into the interrupt?

Each new message's bytes are written to the LCD, with a message count prefix. The display scrolls up,
so we first write the previous message to line 1 and then write the new message to line 2.

Also, the LEDs can be turned on and off in response to Control Change messages.

** MIDI OUT handling ***

Our two pushbuttons and the rotary encoder and its button are controls. When any of them change, the change
is built into a Control Change message and pushed into an outgoing FIFO. That FIFO is byte wide, as the 
MIDI messages are not in the 4-byte USB-MIDI format but rather in standard format, so they are three bytes. 

After the FIFO write is done, we check the status of the UART's built-in transmit FIFO's TXFE (empty) flag.
If it is empty, then transmission must be kicked off. The FIFO is popped and the first byte will be 
written to the transmit FIFO. We'll keep writing to the TX FIFO until we either empty our message FIFO or
the TX FIFO is full. When the TX FIFO drops down to half full (which is the default), we will get interrupted
and we will write more from the message FIFO if there is more left to go.

If after writing to the message FIFO we see that the UART transmit FIFO is not empty, then we can exit and
wait for the interrupt routine to pop the message FIFO.

It is not expected that messages from the user interface will ever be able to swamp the serial transmit port.
If the source of messages is from USB, then we should hopefully be able to NAK USB messages until we can
actually handle them.

**
Most of the code for this can be used by more than one serial port. Each function has as its
first argument a pointer to a structure that holds relevant stuff for the port to consider.
But, since ISRs are specific to a port, and the ISR can't take an argument with that structure,
we need to implement each UART ISR separately, and we also need to make the port structure global
so it can be seen in the ISR. This is where the files midi_uartN.c/.h come in. These are specific
to the UART used for the serial MIDI port, and they implement the ISR and declare the midiport_t
structure.

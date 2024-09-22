#ifndef _LED_H
#define _LED_H

/* Configures the status LED for use. */
void led_initialize();

/*
    Sets the state of the status LED.

    @param is_on Whether the LED should be on or off
*/
void led_set_state(bool is_on);

#endif

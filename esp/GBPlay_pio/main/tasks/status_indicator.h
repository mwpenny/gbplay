#ifndef _STATUS_INDICATOR_H
#define _STATUS_INDICATOR_H

/*
    Updates the status LED based on device state.

    Solid if connected to Wi-Fi, otherwise blinking.
*/
void task_status_indicator_start(int core, int priority);

#endif

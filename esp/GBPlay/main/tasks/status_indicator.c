#include <unistd.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../hardware/led.h"
#include "../hardware/wifi.h"

#define TASK_NAME "status-indicator"

static void task_status_indicator(void* data)
{
    bool led_on = false;

    while (true)
    {
        // Blink if not connected to wifi, otherwise solid
        led_on = wifi_is_connected() || !led_on;
        led_set_state(led_on);

        sleep(1);
    }
}

void task_status_indicator_start(int core, int priority)
{
    xTaskCreatePinnedToCore(
        &task_status_indicator,
        TASK_NAME,
        configMINIMAL_STACK_SIZE,  // Stack size
        NULL,                      // Arguments
        priority,                  // Priority
        NULL,                      // Task handle (output parameter)
        core                       // CPU core ID
    );
}

#include <esp_console.h>
#include <esp_event.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include <unistd.h>

#include "commands.h"
#include "hardware/led.h"
#include "hardware/spi.h"
#include "hardware/storage.h"
#include "hardware/wifi.h"

#include "tasks/network_manager.h"
#include "tasks/status_indicator.h"

#define CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH 1024

void init_console()
{
    esp_console_repl_t* repl = NULL;
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;

    // Prompt to be printed before each line.
    // This can be customized, made dynamic, etc.
    repl_config.prompt = "GBLink >";

    cmds_register();

    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

void start_tasks()
{
    task_network_manager_start();
    task_status_indicator_start();
}

void app_main()
{
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brownout detector

    esp_event_loop_create_default();

    // Initialize hardware
    led_initialize();
    spi_initialize();
    storage_initialize();
    wifi_initialize();

    // Initialize REPL
    init_console();

    // Let's-a-go
    start_tasks();

    while (true)
    {
        sleep(1);
    }

    wifi_deinitialize();
    storage_deinitialize();
    spi_deinitialize();

    esp_event_loop_delete_default();
}

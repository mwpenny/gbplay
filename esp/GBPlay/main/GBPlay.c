#include <esp_console.h>
#include <esp_event.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include <unistd.h>

#include "commands.h"
#include "storage.h"
#include "wifi.h"

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

void app_main()
{
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brownout detector

    esp_event_loop_create_default();

    storage_initialize();
    wifi_initialize();

    init_console();

    while (true)
    {
        usleep(1000 * 1000);
    }

    wifi_deinitialize();
    storage_deinitialize();

    esp_event_loop_delete_default();
}

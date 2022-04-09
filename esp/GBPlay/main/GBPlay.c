#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_pthread.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <sdkconfig.h>
#include <nvs_flash.h>
#include <esp_http_client.h>
#include <esp_netif.h>
#include <esp_tls.h>
#include <esp_crt_bundle.h>
#include <esp_console.h>
#include <esp_vfs_dev.h>
#include <driver/uart.h>
#include <linenoise/linenoise.h>
#include <argtable3/argtable3.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>


#define BLINK_GPIO 5
#define CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH 1024

// WiFi connec tion values
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define ESP_MAXIMUM_RETRY 6
#define DEFAULT_SCAN_LIST_SIZE 6

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;

// WiFi variables
static volatile char* SSID;
static volatile char* SSID_PASS;
static volatile int s_retry_num = 0;
static volatile bool first_init = true;
static volatile bool connection_established = false;
static volatile char address[16];

static volatile struct {
    struct arg_str *SSID;
    struct arg_str *SSID_PASS;
    struct arg_end *end;
} wifi_credentials;

static volatile struct {
    struct arg_str *url;
    struct arg_end *end;
} http_page;

// ESP32 specific values
static volatile esp_err_t err;
static volatile nvs_handle my_handle;

// Other variables
volatile bool state = false;

// Thread function pointers
static void *state_manager(void * arg);
static void *blinker(void * arg);
static void *interface(void * arg);

// Function to change GPIO state
static void blink_led(bool val)
{
    gpio_set_level(BLINK_GPIO, val);
}

// Function to configure GPIO
static void configure_led(void)
{
    printf("Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

//Initialize the Non Volatile Storage
void init_nvs(){
    // Initialize NVS
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
}

//Open NVS to be used by system
void open_nvs(void){
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        printf("Done\n");
    }
}

//Read in SSID and SSID_PASS from NVS
int read_nvs(void){

    int status = -1;

    ESP_LOGI(__func__, "Reading crdentials from NVS ... ");

    size_t required_size;

    nvs_get_str(my_handle, "SSID", NULL, &required_size);
    SSID = malloc(required_size);
    err = nvs_get_str(my_handle, "SSID", SSID, &required_size);

    switch (err) {
        case ESP_OK:
            ESP_LOGI(__func__, "Done");
            ESP_LOGI(__func__, "SSID = %s", SSID);
            status = 0;
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGI(__func__, "The value is not initialized yet!");
            status = 1;
            break;
        default :
            ESP_LOGI(__func__, "Error (%s) reading!", esp_err_to_name(err));
            status = 2;
    }

    nvs_get_str(my_handle, "SSID_PASS", NULL, &required_size);
    SSID_PASS = malloc(required_size);
    err = nvs_get_str(my_handle, "SSID_PASS", SSID_PASS, &required_size);

    switch (err) {
        case ESP_OK:
            ESP_LOGI(__func__, "Done");
            ESP_LOGI(__func__, "SSID_PASS = %s", SSID_PASS);
            status = 0;
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGI(__func__, "The value is not initialized yet!");
            status = 3;
            break;
        default :
            ESP_LOGI(__func__, "Error (%s) reading!", esp_err_to_name(err));
            status = 4;
    }

    return(status);
}

//Write SSID and SSID_PASS to NVS
int write_nvs(void){

    if (!connection_established){
        ESP_LOGI(__func__, "Please establish a connection first");
        return(0);
    }

    // Write WiFi credentials to NVS
    ESP_LOGI(__func__, "Updating credentials in NVS ... ");\
    err = nvs_set_str(my_handle, "SSID", SSID);
    ESP_LOGI(__func__, "%s", ((err != ESP_OK) ? "Failed!\n" : "Done\n"));
    err = nvs_set_str(my_handle, "SSID_PASS", SSID_PASS);
    ESP_LOGI(__func__, "%s", ((err != ESP_OK) ? "Failed!\n" : "Done\n"));

    // Commit written value.
    // After setting any values, nvs_commit() must be called to ensure changes are written
    // to flash storage. Implementations may write to storage at other times,
    // but this is not guaranteed.
    ESP_LOGI(__func__, "Committing updates in NVS ... ");
    err = nvs_commit(my_handle);
    ESP_LOGI(__func__, "%s", ((err != ESP_OK) ? "Failed!\n" : "Done\n"));

    return(0);

}

void wifi_init_all(void){

    if (first_init){
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
        assert(sta_netif);

        first_init = false;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

int wifi_deinit_all(void){
    esp_wifi_stop();
    esp_wifi_deinit();

    connection_established = false;

    return(0);
}

// Scan for WiFi access points and print them to the log
static int wifi_scan(void)
{

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(__func__, "Total APs scanned = %u\n", ap_count);
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        ESP_LOGI(__func__, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(__func__, "RSSI \t\t%d\n", ap_info[i].rssi);
    }

    return(0);

}

// Handler for wifi connection events
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(__func__, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(__func__, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        esp_ip4addr_ntoa(&event->ip_info.ip, address, 16);
        ESP_LOGI(__func__, "got ip: %s", address);
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Initialize a wifi connection using SSID and SSID_PASS
int wifi_init_sta(void)
{

    if(connection_established){
        wifi_deinit_all();
        wifi_init_all();
    }

    s_wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    memcpy(wifi_config.sta.ssid, SSID, strlen(SSID));
    memcpy(wifi_config.sta.password, SSID_PASS, strlen(SSID_PASS));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(__func__, "connected to ap SSID:%s password:%s",
                 SSID, SSID_PASS);
        connection_established = true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(__func__, "Failed to connect to SSID:%s, password:%s",
                 SSID, SSID_PASS);
        s_retry_num = 0;
        wifi_deinit_all();
        wifi_init_all();
        connection_established = false;
    } else {
        ESP_LOGI(__func__, "UNEXPECTED EVENT");
        connection_established = false;
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);

    return(0);
}

// HTTP event handler
esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            printf("\nHTTP_EVENT_ERROR\n");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            printf("\nHTTP_EVENT_ON_CONNECTED\n");
            break;
        case HTTP_EVENT_HEADER_SENT:
            printf("\nHTTP_EVENT_HEADER_SENT\n");
            break;
        case HTTP_EVENT_ON_HEADER:
            printf("\nHTTP_EVENT_ON_HEADER\n");
            printf("\n%.*s\n", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            printf("\nHTTP_EVENT_ON_DATA, len=%d\n", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                printf("%.*s", evt->data_len, (char*)evt->data);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            printf("\nHTTP_EVENT_ON_FINISH\n");
            break;
        case HTTP_EVENT_DISCONNECTED:
            printf("\nHTTP_EVENT_DISCONNECTED\n");
            break;
    }
    return ESP_OK;
}

// Parse an HTTP page
int parse_http_page(int argc, char **argv){

    if(!connection_established){
        ESP_LOGI(__func__, "Please first establish a connection");
        return(1);
    }

    int nerrors = arg_parse(argc, argv, (void **) &http_page);
    if (nerrors != 0) {
        arg_print_errors(stderr, http_page.end, argv[0]);
        return 1;
    }

    esp_http_client_config_t config = {
        .event_handler = _http_event_handle
    };

    config.url = malloc(sizeof(char) * strlen(http_page.url->sval[0]) + 1);
    memcpy(config.url, http_page.url->sval[0], strlen(http_page.url->sval[0]) + 1);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
    ESP_LOGI(__func__, "\nStatus = %d, content_length = %d\n",
            esp_http_client_get_status_code(client),
            esp_http_client_get_content_length(client));
    }
    esp_http_client_cleanup(client);

    return(0);
}

int status(void){

    if(!connection_established){
        ESP_LOGI(__func__, "No connection");
        return(0);
    }

    ESP_LOGI(__func__, "SSID: %s", SSID);
    ESP_LOGI(__func__, "Connection status: %s", connection_established ? "Connected" : "Disconnected");
    ESP_LOGI(__func__, "Address: %s", address);

    return(0);
}

void register_disconnect_wifi(void){
    const esp_console_cmd_t wifi_disconnect_cmd = {
        .command = "disconnect",
        .help = "Disconnect and disable wifi",
        .hint = NULL,
        .func = &wifi_deinit_all
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_disconnect_cmd) );
}

void register_load_page(void){

    http_page.url = arg_str0(NULL, NULL, NULL, NULL);
    http_page.end = arg_end(0);

    const esp_console_cmd_t load_page_cmd = {
        .command = "query",
        .help = "Query an HTTP address and return the result (specify HTTPS)",
        .hint = "query <URL>",
        .func = &parse_http_page,
        .argtable = &http_page
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&load_page_cmd) );

}

void register_save_credentials(void){
    const esp_console_cmd_t wifi_save_cmd = {
        .command = "save",
        .help = "Save current network configuration",
        .hint = NULL,
        .func = &write_nvs
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_save_cmd) );
}

int load_connection(void){

    if(read_nvs() != 0) return(1);
    return(wifi_init_sta());

}

void register_load_credentials(void){
    const esp_console_cmd_t wifi_load_cmd = {
        .command = "load",
        .help = "load saved network configuration",
        .hint = NULL,
        .func = &load_connection
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_load_cmd) );
}

void register_status(void){
    const esp_console_cmd_t wifi_status_cmd = {
        .command = "status",
        .help = "Report WiFi status",
        .hint = NULL,
        .func = &status
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_status_cmd) );
}

void register_wifi_scan(void){
    const esp_console_cmd_t wifi_scan_cmd = {
        .command = "scan",
        .help = "Scan available WiFi networks (returns the strongest 6)",
        .hint = NULL,
        .func = &wifi_scan
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_scan_cmd) );
}

int wifi_connect(int argc, char **argv){

    int nerrors = arg_parse(argc, argv, (void **) &wifi_credentials);
    if (nerrors != 0) {
        arg_print_errors(stderr, wifi_credentials.end, argv[0]);
        return 1;
    }

    free(SSID);
    free(SSID_PASS);

    SSID = malloc(sizeof(char) * strlen(wifi_credentials.SSID->sval[0])+1);
    SSID_PASS = malloc(sizeof(char) * strlen(wifi_credentials.SSID_PASS->sval[0])+1);

    memcpy(SSID, wifi_credentials.SSID->sval[0], strlen(wifi_credentials.SSID->sval[0]));
    memcpy(SSID_PASS, wifi_credentials.SSID_PASS->sval[0], strlen(wifi_credentials.SSID_PASS->sval[0]));

    SSID[strlen(wifi_credentials.SSID->sval[0])] = '\0';
    SSID_PASS[strlen(wifi_credentials.SSID_PASS->sval[0])] = '\0';

    return(wifi_init_sta());
}

void register_wifi_connect(void){

    wifi_credentials.SSID = arg_str1(NULL, NULL, NULL, NULL);
    wifi_credentials.SSID_PASS = arg_str1(NULL, NULL, NULL, NULL);
    wifi_credentials.end = arg_end(1);

    const esp_console_cmd_t wifi_connect_cmd = {
        .command = "connect",
        .help = "Connect to WiFi network using specified credentials",
        .hint = "connect <SSID> <SSID Password>",
        .func = &wifi_connect,
        .argtable = &wifi_credentials
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_connect_cmd) );
}

void init_console(void){
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = "GBLink >";
    repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;

    esp_console_register_help_command();
    register_wifi_scan();
    register_wifi_connect();
    register_status();
    register_save_credentials();
    register_load_credentials();
    register_load_page();
    register_disconnect_wifi();

    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

// Main control loop
void app_main(void)
{

    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

    init_nvs();
    open_nvs();

    wifi_init_all();

    init_console();

    while(true){
        usleep(1000 * 1000);
    }

    // Close
    nvs_close(my_handle);

    wifi_deinit_all();

    //read_nvs();
    
    //SSID = malloc(sizeof(char) * 5);
    //SSID_PASS = malloc(sizeof(char) * 13);
    //strcpy(SSID, "Craig");
    //strcpy(SSID_PASS, "Portogruaro64");
    //printf("%s\n", SSID);
    //printf("%s\n", SSID_PASS);

    //nvs_flash_erase();
    //write_nvs();

    //read_nvs();

    //ESP_ERROR_CHECK(esp_netif_init());
    //ESP_ERROR_CHECK(esp_event_loop_create_default());
    //esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    //assert(sta_netif);

    //wifi_scan();

    //wifi_init_sta();

    //parse_http_page();

    /*

    wifi_deinit_all();

    wifi_scan();

    // Close
    nvs_close(my_handle);

    configure_led();

    */

    /*

    pthread_attr_t attr;
    //pthread_t thread1, thread2;
    pthread_t interface_thread, state_manager_thread;
    esp_pthread_cfg_t esp_pthread_cfg;
    int res;

    res = pthread_create(&interface_thread, NULL, interface, NULL);
    assert(res == 0);
    printf("Created thread 0x%x\n", interface_thread);

    res = pthread_create(&state_manager_thread, NULL, state_manager, NULL);
    assert(res == 0);
    printf("Created thread 0x%x\n", state_manager_thread);

    */

    /*

    // Create a pthread with the default parameters
    res = pthread_create(&thread1, NULL, blinker, NULL);
    assert(res == 0);
    printf("Created thread 0x%x\n", thread1);

    // Create a pthread with a larger stack size using the standard API
    res = pthread_attr_init(&attr);
    assert(res == 0);
    pthread_attr_setstacksize(&attr, 16384);
    res = pthread_create(&thread2, &attr, state_manager, NULL);
    assert(res == 0);
    printf("Created larger stack thread 0x%x\n", thread2);

    */

    //res = pthread_join(interface_thread, NULL);
    //assert(res = 0);
    //res = pthread_join(state_manager_thread, NULL);
    //assert(res = 0);

    //printf("Threads have exited");

    /*

    res = pthread_join(thread1, NULL);
    assert(res == 0);
    res = pthread_join(thread2, NULL);
    assert(res == 0);
    printf("Threads have exited\n\n");

    */

    // Use the ESP-IDF API to change the default thread attributes
    // esp_pthread_cfg = esp_pthread_get_default_config();
    // esp_pthread_cfg.stack_size = 32768;
    // esp_pthread_cfg.prio += 2;
    // ESP_ERROR_CHECK( esp_pthread_set_cfg(&esp_pthread_cfg) );

    // res = pthread_create(&thread1, NULL, example_thread, NULL);
    // assert(res == 0);
    // printf("Created thread 0x%x with new default config\n", thread1);
    // res = pthread_join(thread1, NULL);
    // assert(res == 0);
    // printf("Thread has exited\n\n");

}

static void *state_manager(void * arg)
{
    usleep(250 * 1000);
    printf("This thread has ID 0x%x and %u bytes free stack\n", pthread_self(), uxTaskGetStackHighWaterMark(NULL));

    while(true){
        state = !state;
        ESP_LOGI(__func__, "\nbop");
        usleep(1000 * 1000);
    }

    return NULL;
}

static void *blinker(void * arg)
{
    usleep(250 * 1000);
    printf("This thread has ID 0x%x and %u bytes free stack\n", pthread_self(), uxTaskGetStackHighWaterMark(NULL));

    while(true){
        blink_led(state);
        usleep(250 * 1000);
    }

    return NULL;
}

static void *interface(void * args){
    return NULL;
}
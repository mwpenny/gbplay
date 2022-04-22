#include <unistd.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_http_client.h>
#include <esp_log.h>

#include "../hardware/led.h"
#include "../hardware/wifi.h"
#include "socket_manager.h"

#define TASK_NAME "socket-manager"

char rx_buffer[16];
volatile bool socket_data = false;

int sock = -1;
struct sockaddr_in dest_addr;

static void task_socket_manager(void* data)
{

    dest_addr.sin_addr.s_addr = inet_addr("192.168.0.164");
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(9898);

    while(!wifi_is_connected()){
        sleep(1);
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(__func__, "Unable to create socket: errno %d", errno);
    }
    ESP_LOGI(__func__, "Socket created!");

    connect_socket();

    while(true){
        while (socket_data == true) sleep(1);
        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        // Error occurred during receiving
        if (len < 0) {
            ESP_LOGE(__func__, "recv failed: errno %d", errno);
        }
        // Data received
        else {
            rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
            socket_data = true;
            ESP_LOGI(__func__, "Data has arrived!");
        }

        sleep(1);
    }
}

bool connect_socket(){
    int error = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
        if (error != 0) {
            ESP_LOGE(__func__, "Socket unable to connect: errno %d", errno);
            return false;
        }
        ESP_LOGI(__func__, "Successfully connected");
        return true;
}

bool socket_send_data(const char* data){

    if (sock < 0) return false;

    int error = send(sock, data, strlen(data), 0);
    if (error < 0) {
        ESP_LOGE(__func__, "Error occurred during sending: errno %d", errno);
        return false;
    }

    return true;
}

void task_socket_manager_start()
{
    xTaskCreatePinnedToCore(
        &task_socket_manager,
        TASK_NAME,
        configMINIMAL_STACK_SIZE*4,  // Stack size (in words)
        NULL,                      // Arguments
        0,                         // Priority
        NULL,                      // Task handle (output parameter)
        1                          // CPU core ID
    );
}

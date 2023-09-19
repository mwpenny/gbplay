# TODO: split up into separate components
idf_component_register(
    SRCS "GBPlay.c" "commands.c" "http.c" "socket.c" "hardware/led.c" "hardware/spi.c" "hardware/storage.c" "hardware/wifi.c" "tasks/network_manager.c" "tasks/socket_manager.c" "tasks/status_indicator.c"
    INCLUDE_DIRS "."
)

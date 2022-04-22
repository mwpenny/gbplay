# TODO: split up into separate components
idf_component_register(
    SRCS "GBPlay.c" "commands.c" "http.c" "hardware/led.c" "hardware/storage.c" "hardware/wifi.c" "tasks/status_indicator.c" "tasks/socket_manager.c"
    INCLUDE_DIRS "."
)

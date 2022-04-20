# TODO: split up into separate components
idf_component_register(
    SRCS "GBPlay.c" "commands.c" "http.c" "hardware/led.c" "hardware/storage.c" "hardware/wifi.c" "tasks/connection_manager.c" "tasks/status_indicator.c"
    INCLUDE_DIRS "."
)

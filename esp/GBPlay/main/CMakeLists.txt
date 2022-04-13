# TODO: split up into separate components
idf_component_register(
    SRCS "GBPlay.c" "commands.c" "http.c" "hardware/storage.c" "hardware/wifi.c"
    INCLUDE_DIRS "."
)

#include <driver/spi_master.h>
#include <soc/spi_pins.h>

#include "spi.h"

#define GB_SPI_MODE 3
#define GB_SPI_CLOCK_SPEED_HZ 8000

#define GB_SPI_HOST SPI2_HOST
#define GB_PIN_MOSI SPI2_IOMUX_PIN_NUM_MOSI  // Pin 13
#define GB_PIN_MISO SPI2_IOMUX_PIN_NUM_MISO  // Pin 12
#define GB_PIN_SCLK SPI2_IOMUX_PIN_NUM_CLK   // Pin 14

static spi_device_handle_t _spi_slave_handle = NULL;

void spi_initialize()
{
    spi_bus_config_t bus_config = {
        .mosi_io_num = GB_PIN_MOSI,
        .miso_io_num = GB_PIN_MISO,
        .sclk_io_num = GB_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .intr_flags = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(GB_SPI_HOST, &bus_config, SPI_DMA_DISABLED));

    spi_device_interface_config_t dev_config = {0};
    dev_config.mode = GB_SPI_MODE;
    dev_config.clock_speed_hz = GB_SPI_CLOCK_SPEED_HZ;
    dev_config.spics_io_num = -1;
    dev_config.queue_size = 1;
    ESP_ERROR_CHECK(spi_bus_add_device(GB_SPI_HOST, &dev_config, &_spi_slave_handle));
}

void spi_deinitialize()
{
    spi_bus_remove_device(_spi_slave_handle);
    spi_bus_free(GB_SPI_HOST);
}

uint8_t spi_exchange_byte(uint8_t tx)
{
    uint8_t rx = 0xFF;

    spi_transaction_t txn = {0};
    txn.length = 8;  // In bits
    txn.tx_buffer = &tx;
    txn.rx_buffer = &rx;

    ESP_ERROR_CHECK(spi_device_transmit(_spi_slave_handle, &txn));
    return rx;
}

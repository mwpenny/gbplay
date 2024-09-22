#ifndef _SPI_H
#define _SPI_H


#include <unistd.h>
#include <stdint.h>

/* Configures the SPI interface for use. */
void spi_initialize();

/* Disables the SPI interface. */
void spi_deinitialize();

/*
    Exchanges a byte with the SPI slave device.

    @param tx The byte to send

    @returns The received byte, or 0xFF if there was no response
*/
uint8_t spi_exchange_byte(uint8_t tx);

#endif

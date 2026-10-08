#pragma once
// nice!view LS011B7DH03 hardware SPI0: GP1=SCS (active high), GP2=SCK, GP3=MOSI.
#define ADBK_DISPLAY_CS_PIN GP1

#define SPI_DRIVER SPID0
#define SPI_SCK_PIN GP2
#define SPI_MOSI_PIN GP3
#define SPI_MISO_PIN NO_PIN

#define SPI_SELECT_MODE SPI_SELECT_MODE_NONE

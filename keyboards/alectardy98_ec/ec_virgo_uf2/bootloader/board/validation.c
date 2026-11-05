// SPDX-License-Identifier: MIT
#include "board_api.h"
#include "validation.h"
#include <stdio.h>
#include <string.h>

static bool geometry_failed;
static uint32_t boot_request, boot_reset_flags;
extern uint32_t _board_dfu_dbl_tap[];

// Observe before TinyUF2 consumes the request or changes its double-tap marker.
void board_init2(void) {
  boot_request = _board_dfu_dbl_tap[0];
  boot_reset_flags = RCC->CSR;
}

void virgo_port_dfu_init(void);
void board_dfu_init(void) {
  extern char infoUf2File[384];
  volatile const uint32_t *v = (volatile const uint32_t *)BOARD_FLASH_APP_START;
  // Preserve the model/version prefix, then let uf2_init append flash capacity.
  const char *marker = "Flash Size: 0x";
  char *tail = strstr(infoUf2File, marker);
  if (tail) {
    size_t space = sizeof(infoUf2File) - (size_t)(tail - infoUf2File);
    snprintf(tail, space,
             "Factory KiB: %u\r\nSP: %08lX Reset: %08lX\r\n"
             "Vectors valid: %u\r\nRequest: %08lX RCC_CSR: %08lX\r\n"
             "Write gate fault: %u\r\nFlash Size: 0x",
             (unsigned)*(volatile const uint16_t *)FLASHSIZE_BASE,
             (unsigned long)v[0], (unsigned long)v[1],
             (unsigned)virgo_vectors_valid(v[0], v[1]),
             (unsigned long)boot_request, (unsigned long)boot_reset_flags,
             (unsigned)geometry_failed);
  }
  virgo_port_dfu_init();
}
bool virgo_port_flash_write(uint32_t address, void const *data, uint32_t length);

bool board_app_valid(void) {
  volatile const uint32_t *vectors = (volatile const uint32_t *)BOARD_FLASH_APP_START;
  return *(volatile const uint16_t *)FLASHSIZE_BASE == 256 &&
         virgo_vectors_valid(vectors[0], vectors[1]);
}

bool board_flash_write(uint32_t address, void const *data, uint32_t length) {
  if (!data || !virgo_write_valid(address, length,
                                 *(volatile const uint16_t *)FLASHSIZE_BASE)) {
    geometry_failed = true;
    return false;
  }
  return virgo_port_flash_write(address, data, length);
}

// Keep recovery available after this adapter rejects a transfer too.
bool board_app_valid2(void) {
  return !geometry_failed;
}

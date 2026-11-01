/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_storage_caps.h"
#include <string.h>
my2k_storage_caps my2k_storage_negotiate(uint16_t vid, uint16_t pid, bool device_valid, bool inquiry_valid,
                                         const uint8_t inq[36]) {
    my2k_storage_caps no = {0, 0};
    if (!device_valid || !inquiry_valid || !inq || vid != 0x1209 || pid != 1 || inq[0] != 0 ||
        memcmp(inq + 8, "MY2K-EXP", 8) ||
        (memcmp(inq + 16, "Read-only assets", 16)
#if MY2K_RELEASE1_PROFILE
         && (memcmp(inq + 16, "R1 asset storage", 16) || memcmp(inq + 32, "R001", 4))
#endif
             ))
        return no;
    if (!memcmp(inq + 32, "0007", 4))
        return (my2k_storage_caps){7, 512};
    if (!memcmp(inq + 32, "D015", 4))
        return (my2k_storage_caps){15, 4096};
#if MY2K_DEV21_PROFILE
    if (!memcmp(inq + 32, "D021", 4))
        return (my2k_storage_caps){21, 4096};
#endif
#if MY2K_RELEASE1_PROFILE
    if (!memcmp(inq + 32, "R001", 4) && !memcmp(inq + 16, "R1 asset storage", 16))
        return (my2k_storage_caps){24, 4096};
#endif
    return no;
}

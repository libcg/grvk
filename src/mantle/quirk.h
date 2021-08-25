#ifndef QUIRK_H_
#define QUIRK_H_

#include "mantle_internal.h"

typedef enum {
    // Game crashes when size and alignment are zero for objects with no memory requirements
    QUIRK_NON_ZERO_MEM_REQ = 1,

    // Read-only image bound to UAV slot
    QUIRK_READ_ONLY_IMAGE_BOUND_TO_UAV = 2,

    // Image not transitioned to GR_IMAGE_STATE_CLEAR before a clear
    QUIRK_IMAGE_WRONG_CLEAR_STATE = 4,

    // Image copy offset/extent provided in texels instead of blocks
    QUIRK_COMPRESSED_IMAGE_COPY_IN_TEXELS = 8,

    // Command buffer reset before completion
    QUIRK_INVALID_CMD_BUFFER_RESET = 16,

    // Cubemap layer index and count are divided by 6
    QUIRK_CUBEMAP_LAYER_DIV_6 = 32,
} QUIRK_FLAGS;

void quirkInit(
    const GR_APPLICATION_INFO* appInfo);

bool quirkHas(
    QUIRK_FLAGS flags);

#endif // QUIRK_H_

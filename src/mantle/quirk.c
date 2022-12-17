#include "quirk.h"

static QUIRK_FLAGS mQuirks = 0;

void quirkInit(
    const GR_APPLICATION_INFO* appInfo)
{
    if (appInfo->pAppName == NULL || appInfo->pEngineName == NULL) {
        return;
    }

    if (!strcmp(appInfo->pAppName, "Star Swarm")) {
        mQuirks = QUIRK_NON_ZERO_MEM_REQ |
                  QUIRK_READ_ONLY_IMAGE_STATE_MISMATCH |
                  QUIRK_IMAGE_DATA_TRANSFER_STATE_FOR_RAW_CLEAR |
                  QUIRK_COMPRESSED_IMAGE_COPY_IN_TEXELS |
                  QUIRK_INVALID_CMD_BUFFER_RESET |
                  QUIRK_CUBEMAP_LAYER_DIV_6 |
                  QUIRK_SILENCE_TRANSFER_ONLY_LINEAR_IMAGE_WARNINGS |
                  QUIRK_DESCRIPTOR_SET_USE_DEDICATED_ALLOCATION |
                  QUIRK_DESCRIPTOR_SET_INTERNAL_SYNCHRONIZED;
    } else if (!strcmp(appInfo->pAppName, "Battlefield")) {
        mQuirks = QUIRK_GRAPHICS_PIPELINE_FORMAT_MISMATCH;
    } else if (!strcmp(appInfo->pEngineName, "CivTech")) {
        mQuirks = QUIRK_NON_ZERO_MEM_REQ |
                  QUIRK_READ_ONLY_IMAGE_STATE_MISMATCH |
                  QUIRK_KEEP_VK_DEVICE;
    }

    if (mQuirks != 0) {
        LOGI("enabled 0x%X\n", mQuirks);
    }
}

bool quirkHas(
    QUIRK_FLAGS flags) {
    return (mQuirks & flags) == flags;
}

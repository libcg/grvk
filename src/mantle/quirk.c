#include "quirk.h"

static QUIRK_FLAGS mQuirks = 0;

void quirkInit(
    const GR_APPLICATION_INFO* appInfo)
{
    if (!strcmp(appInfo->pAppName, "Star Swarm")) {
        mQuirks = QUIRK_NON_ZERO_MEM_REQ |
                  QUIRK_READ_ONLY_IMAGE_BOUND_TO_UAV |
                  QUIRK_IMAGE_WRONG_CLEAR_STATE |
                  QUIRK_COMPRESSED_IMAGE_COPY_IN_TEXELS |
                  QUIRK_INVALID_CMD_BUFFER_RESET;
    }

    if (mQuirks != 0) {
        LOGI("enabled 0x%X\n", mQuirks);
    }
}

bool quirkHas(
    QUIRK_FLAGS flags) {
    return (mQuirks & flags) == flags;
}

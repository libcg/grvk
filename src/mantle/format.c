#include <stdio.h>
#include "mantle_internal.h"

#define PACK_FORMAT(channel, numeric) \
    ((channel) << 16 | (numeric))

VkFormat grFormatToVk(
    GR_FORMAT format)
{
    uint32_t packed = PACK_FORMAT(format.channelFormat, format.numericFormat);

    switch (packed) {
    case PACK_FORMAT(GR_CH_FMT_R8G8B8A8, GR_NUM_FMT_UNORM):
        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    printf("%s: unsupported format %u %u\n", __func__, format.channelFormat, format.numericFormat);
    return VK_FORMAT_UNDEFINED;
}

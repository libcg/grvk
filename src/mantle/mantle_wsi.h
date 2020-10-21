#ifndef GR_MANTLE_WSI_H
#define GR_MANTLE_WSI_H

#include "mantle_object.h"
#include "vulkan/vulkan.h"

typedef struct _GrWsiImage {
    GrStructType sType;
    VkImage image;
    VkExtent2D extent;
    VkDeviceMemory imageMemory;

    VkFence fence;
    VkCommandBuffer copyCmdBuf;
    VkBuffer copyBuffer;
    VkDeviceMemory bufferMemory;
    void* bufferMemoryPtr;
    size_t bufferSize;
    size_t bufferRowPitch;
    VkFormat format;
} GrWsiImage;

#endif

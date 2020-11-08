#ifndef GR_MANTLE_WSI_H
#define GR_MANTLE_WSI_H

#include "mantle_object.h"
#include "vulkan/vulkan.h"

typedef struct _GrWsiImage {
    GrStructType sType;
    GrDevice* device;
    VkImage image;
    VkExtent3D extent;
    uint32_t layerCount;
    VkFormat format;
    VkDeviceMemory imageMemory;

    VkFence fence;
    VkCommandBuffer copyCmdBuf;
    VkBuffer copyBuffer;
    VkDeviceMemory bufferMemory;
    void* bufferMemoryPtr;
    size_t bufferSize;
    size_t bufferRowPitch;
} GrWsiImage;

#endif

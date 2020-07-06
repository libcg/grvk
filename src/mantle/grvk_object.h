#ifndef GRVK_OBJECT_H_
#define GRVK_OBJECT_H_

#include "vulkan/vulkan.h"

typedef enum _GrvkStructType {
    GRVK_STRUCT_TYPE_COMMAND_BUFFER,
    GRVK_STRUCT_TYPE_COLOR_BLEND_STATE_OBJECT,
    GRVK_STRUCT_TYPE_COLOR_TARGET_VIEW,
    GRVK_STRUCT_TYPE_DEPTH_STENCIL_STATE_OBJECT,
    GRVK_STRUCT_TYPE_DESCRIPTOR_SET,
    GRVK_STRUCT_TYPE_DEVICE,
    GRVK_STRUCT_TYPE_FENCE,
    GRVK_STRUCT_TYPE_GPU_MEMORY,
    GRVK_STRUCT_TYPE_IMAGE,
    GRVK_STRUCT_TYPE_MSAA_STATE_OBJECT,
    GRVK_STRUCT_TYPE_PHYSICAL_GPU,
    GRVK_STRUCT_TYPE_PIPELINE,
    GRVK_STRUCT_TYPE_RASTER_STATE_OBJECT,
    GRVK_STRUCT_TYPE_SHADER,
    GRVK_STRUCT_TYPE_QUEUE,
    GRVK_STRUCT_TYPE_VIEWPORT_STATE_OBJECT,
} GrvkStructType;

// Generic object used to read the object type
typedef struct _GrvkObject {
    GrvkStructType sType;
} GrvkObject;

typedef struct _GrvkCmdBuffer {
    GrvkStructType sType;
    VkCommandBuffer commandBuffer;
} GrvkCmdBuffer;

typedef struct _GrvkColorBlendStateObject {
    GrvkStructType sType;
    VkPipelineColorBlendStateCreateInfo* colorBlendStateCreateInfo;
} GrvkColorBlendStateObject;

typedef struct _GrvkColorTargetView {
    GrvkStructType sType;
    VkImageView imageView;
} GrvkColorTargetView;

typedef struct _GrvkDepthStencilStateObject {
    GrvkStructType sType;
    VkPipelineDepthStencilStateCreateInfo* depthStencilStateCreateInfo;
} GrvkDepthStencilStateObject;

typedef struct _GrvkDescriptorSet {
    GrvkStructType sType;
    uint32_t slotCount; // Unused
} GrvkDescriptorSet;

typedef struct _GrvkDevice {
    GrvkStructType sType;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
} GrvkDevice;

typedef struct _GrvkFence {
    GrvkStructType sType;
    VkFence fence;
} GrvkFence;

typedef struct _GrvkGpuMemory {
    GrvkStructType sType;
    VkDeviceMemory deviceMemory;
    VkDevice device;
} GrvkGpuMemory;

typedef struct _GrvkImage {
    GrvkStructType sType;
    VkImage image;
} GrvkImage;

typedef struct _GrvkMsaaStateObject {
    GrvkStructType sType;
    VkPipelineMultisampleStateCreateInfo* multisampleStateCreateInfo;
} GrvkMsaaStateObject;

typedef struct _GrvkPhysicalGpu {
    GrvkStructType sType;
    VkPhysicalDevice physicalDevice;
} GrvkPhysicalGpu;

typedef struct _GrvkPipeline {
    GrvkStructType sType;
    VkGraphicsPipelineCreateInfo* graphicsPipelineCreateInfo;
} GrvkPipeline;

typedef struct _GrvkRasterStateObject {
    GrvkStructType sType;
    VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo;
} GrvkRasterStateObject;

typedef struct _GrvkShader {
    GrvkStructType sType;
    VkShaderModule shaderModule;
} GrvkShader;

typedef struct _GrvkQueue {
    GrvkStructType sType;
    VkQueue queue;
} GrvkQueue;

typedef struct _GrvkViewportStateObject {
    GrvkStructType sType;
    VkPipelineViewportStateCreateInfo* viewportStateCreateInfo;
} GrvkViewportStateObject;

#endif // GRVK_OBJECT_H_

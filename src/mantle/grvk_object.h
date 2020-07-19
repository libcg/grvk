#ifndef GRVK_OBJECT_H_
#define GRVK_OBJECT_H_

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#define MAX_STAGE_COUNT 5 // VS, HS, DS, GS, PS

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

typedef struct _GrvkDescriptorSet GrvkDescriptorSet;
typedef struct _GrvkPipeline GrvkPipeline;

// Generic object used to read the object type
typedef struct _GrvkObject {
    GrvkStructType sType;
} GrvkObject;

typedef struct _GrvkCmdBuffer {
    GrvkStructType sType;
    VkCommandBuffer commandBuffer;
    GrvkPipeline* boundPipeline;
    GrvkDescriptorSet* descriptorSet;
    bool descriptorSetIsBound;
} GrvkCmdBuffer;

typedef struct _GrvkColorBlendStateObject {
    GrvkStructType sType;
    float blendConstants[4];
} GrvkColorBlendStateObject;

typedef struct _GrvkColorTargetView {
    GrvkStructType sType;
    VkImageView imageView;
} GrvkColorTargetView;

typedef struct _GrvkDepthStencilStateObject {
    GrvkStructType sType;
    VkBool32 depthTestEnable;
    VkBool32 depthWriteEnable;
    VkCompareOp depthCompareOp;
    VkBool32 depthBoundsTestEnable;
    VkBool32 stencilTestEnable;
    VkStencilOpState front;
    VkStencilOpState back;
    float minDepthBounds;
    float maxDepthBounds;
} GrvkDepthStencilStateObject;

typedef struct _GrvkDescriptorSet {
    GrvkStructType sType;
    VkDevice device;
    VkDescriptorPool descriptorPool;
    void* slots;
    uint32_t slotCount;
    VkDescriptorSet descriptorSets[MAX_STAGE_COUNT];
} GrvkDescriptorSet;

typedef struct _GrvkDevice {
    GrvkStructType sType;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    uint32_t universalQueueIndex;
    VkCommandPool universalCommandPool;
    uint32_t computeQueueIndex;
    VkCommandPool computeCommandPool;
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
} GrvkMsaaStateObject;

typedef struct _GrvkPhysicalGpu {
    GrvkStructType sType;
    VkPhysicalDevice physicalDevice;
} GrvkPhysicalGpu;

typedef struct _GrvkPipeline {
    GrvkStructType sType;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
} GrvkPipeline;

typedef struct _GrvkRasterStateObject {
    GrvkStructType sType;
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
    float depthBiasConstantFactor;
    float depthBiasClamp;
    float depthBiasSlopeFactor;
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
    VkViewport* viewports;
    uint32_t viewportCount;
    VkRect2D* scissors;
    uint32_t scissorCount;
} GrvkViewportStateObject;

#endif // GRVK_OBJECT_H_

#ifndef GR_OBJECT_H_
#define GR_OBJECT_H_

#include "mantle/mantle.h"
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#define MAX_STAGE_COUNT 5 // VS, HS, DS, GS, PS

typedef enum _GrStructType {
    GR_STRUCT_TYPE_COMMAND_BUFFER,
    GR_STRUCT_TYPE_COLOR_BLEND_STATE_OBJECT,
    GR_STRUCT_TYPE_COLOR_TARGET_VIEW,
    GR_STRUCT_TYPE_DEPTH_STENCIL_STATE_OBJECT,
    GR_STRUCT_TYPE_DESCRIPTOR_SET,
    GR_STRUCT_TYPE_DEVICE,
    GR_STRUCT_TYPE_FENCE,
    GR_STRUCT_TYPE_EVENT,
    GR_STRUCT_TYPE_QUEUE_SEMAPHORE,
    GR_STRUCT_TYPE_GPU_MEMORY,
    GR_STRUCT_TYPE_IMAGE,
    GR_STRUCT_TYPE_MSAA_STATE_OBJECT,
    GR_STRUCT_TYPE_PHYSICAL_GPU,
    GR_STRUCT_TYPE_PIPELINE,
    GR_STRUCT_TYPE_RASTER_STATE_OBJECT,
    GR_STRUCT_TYPE_SAMPLER,
    GR_STRUCT_TYPE_SHADER,
    GR_STRUCT_TYPE_QUEUE,
    GR_STRUCT_TYPE_VIEWPORT_STATE_OBJECT,
} GrStructType;

typedef struct _GrDescriptorSet GrDescriptorSet;
typedef struct _GrPipeline GrPipeline;

// Generic object used to read the object type
typedef struct _GrObject {
    GrStructType sType;
} GrObject;

typedef struct _GrCmdBuffer {
    GrStructType sType;
    VkCommandBuffer commandBuffer;
    GrPipeline* grPipeline;
    GrDescriptorSet* grDescriptorSet;
    GR_COLOR_TARGET_BIND_INFO colorTargets[GR_MAX_COLOR_TARGETS];
    uint32_t colorTargetCount;
    GR_DEPTH_STENCIL_BIND_INFO depthTarget;
    bool hasDepthTarget;
    bool hasActiveRenderPass;
    bool isDirty;
} GrCmdBuffer;

typedef struct _GrColorBlendStateObject {
    GrStructType sType;
    float blendConstants[4];
} GrColorBlendStateObject;

typedef struct _GrColorTargetView {
    GrStructType sType;
    VkImageView imageView;
    VkExtent3D extent;
} GrColorTargetView;

typedef struct _GrDepthStencilStateObject {
    GrStructType sType;
    VkBool32 depthTestEnable;
    VkBool32 depthWriteEnable;
    VkCompareOp depthCompareOp;
    VkBool32 depthBoundsTestEnable;
    VkBool32 stencilTestEnable;
    VkStencilOpState front;
    VkStencilOpState back;
    float minDepthBounds;
    float maxDepthBounds;
} GrDepthStencilStateObject;

typedef struct _GrDescriptorSet {
    GrStructType sType;
    VkDevice device;
    VkDescriptorPool descriptorPool;
    void* slots;
    uint32_t slotCount;
    VkDescriptorSet descriptorSets[MAX_STAGE_COUNT];
} GrDescriptorSet;

typedef struct _GrDevice {
    GrStructType sType;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    uint32_t universalQueueIndex;
    VkCommandPool universalCommandPool;
    uint32_t computeQueueIndex;
    VkCommandPool computeCommandPool;
} GrDevice;

typedef struct _GrFence {
    GrStructType sType;
    VkDevice device;
    VkFence fence;
} GrFence;

typedef struct _GrEvent {
    GrStructType sType;
    VkDevice device;
    VkEvent event;
} GrEvent;

typedef struct _GrSemaphore {
    GrStructType sType;
    VkSemaphore semaphore;
} GrSemaphore;

typedef struct _GrGpuMemory {
    GrStructType sType;
    VkDeviceMemory deviceMemory;
    VkDevice device;
    VkBuffer buffer;
} GrGpuMemory;

typedef struct _GrImage {
    GrStructType sType;
    VkImage image;
    VkExtent2D extent;
} GrImage;

typedef struct _GrMsaaStateObject {
    GrStructType sType;
} GrMsaaStateObject;

typedef struct _GrPhysicalGpu {
    GrStructType sType;
    VkPhysicalDevice physicalDevice;
} GrPhysicalGpu;

typedef struct _GrPipeline {
    GrStructType sType;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    VkRenderPass renderPass;
} GrPipeline;

typedef struct _GrRasterStateObject {
    GrStructType sType;
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
    float depthBiasConstantFactor;
    float depthBiasClamp;
    float depthBiasSlopeFactor;
} GrRasterStateObject;

typedef struct _GrSampler {
    GrStructType sType;
    VkSampler sampler;
} GrSampler;

typedef struct _GrShader {
    GrStructType sType;
    VkShaderModule shaderModule;
} GrShader;

typedef struct _GrQueue {
    GrStructType sType;
    GrDevice* grDevice;
    VkQueue queue;
    uint32_t queueIndex;
} GrQueue;

typedef struct _GrViewportStateObject {
    GrStructType sType;
    VkViewport* viewports;
    uint32_t viewportCount;
    VkRect2D* scissors;
    uint32_t scissorCount;
} GrViewportStateObject;

#endif // GR_OBJECT_H_

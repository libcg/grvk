#ifndef GR_OBJECT_H_
#define GR_OBJECT_H_

#include <stdbool.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "vulkan_loader.h"
#include "mantle/mantle.h"
#include "amdilc.h"

#include "vk_mem_alloc.h"

#define MAX_STAGE_COUNT     5 // VS, HS, DS, GS, PS
#define MSAA_LEVEL_COUNT    5 // 1, 2, 4, 8, 16x

#define UNIVERSAL_ATOMIC_COUNTERS_COUNT (512)
#define COMPUTE_ATOMIC_COUNTERS_COUNT   (1024)

#define IMAGE_PREP_CMD_BUFFER_COUNT     (16)

#define GET_OBJ_TYPE(obj) \
    (((GrBaseObject*)(obj))->grObjType)

#define GET_OBJ_DEVICE(obj) \
    (((GrObject*)(obj))->grDevice)

typedef enum _GrObjectType {
    GR_OBJ_TYPE_BORDER_COLOR_PALETTE,
    GR_OBJ_TYPE_COMMAND_BUFFER,
    GR_OBJ_TYPE_COLOR_BLEND_STATE_OBJECT,
    GR_OBJ_TYPE_COLOR_TARGET_VIEW,
    GR_OBJ_TYPE_DEPTH_STENCIL_STATE_OBJECT,
    GR_OBJ_TYPE_DEPTH_STENCIL_VIEW,
    GR_OBJ_TYPE_DESCRIPTOR_SET,
    GR_OBJ_TYPE_DEVICE,
    GR_OBJ_TYPE_EVENT,
    GR_OBJ_TYPE_FENCE,
    GR_OBJ_TYPE_GPU_MEMORY,
    GR_OBJ_TYPE_IMAGE,
    GR_OBJ_TYPE_IMAGE_VIEW,
    GR_OBJ_TYPE_MSAA_STATE_OBJECT,
    GR_OBJ_TYPE_PHYSICAL_GPU,
    GR_OBJ_TYPE_PIPELINE,
    GR_OBJ_TYPE_QUERY_POOL,
    GR_OBJ_TYPE_QUEUE,
    GR_OBJ_TYPE_QUEUE_SEMAPHORE,
    GR_OBJ_TYPE_RASTER_STATE_OBJECT,
    GR_OBJ_TYPE_SAMPLER,
    GR_OBJ_TYPE_SHADER,
    GR_OBJ_TYPE_VIEWPORT_STATE_OBJECT,
    GR_OBJ_TYPE_WSI_WIN_DISPLAY,
} GrObjectType;

typedef enum _DescriptorSetSlotType
{
    SLOT_TYPE_NONE,
    SLOT_TYPE_IMAGE,
    SLOT_TYPE_BUFFER,
    SLOT_TYPE_NESTED,
} DescriptorSetSlotType;

typedef struct _GrColorBlendStateObject GrColorBlendStateObject;
typedef struct _GrDepthStencilStateObject GrDepthStencilStateObject;
typedef struct _GrDescriptorSet GrDescriptorSet;
typedef struct _GrDevice GrDevice;
typedef struct _GrFence GrFence;
typedef struct _GrGpuMemory GrGpuMemory;
typedef struct _GrMsaaStateObject GrMsaaStateObject;
typedef struct _GrPipeline GrPipeline;
typedef struct _GrQueue GrQueue;
typedef struct _GrRasterStateObject GrRasterStateObject;
typedef struct _GrViewportStateObject GrViewportStateObject;

typedef struct _DescriptorSetSlot
{
    DescriptorSetSlotType type;
    union {
        struct {
            VkDescriptorImageInfo imageInfo;
            VkImage image;
            bool isLinearImageWithBuffer;
        } image;
        struct {
            VkBufferView bufferView;
            VkDescriptorBufferInfo bufferInfo;
            VkDeviceSize stride;
        } buffer;
        struct {
            const GrDescriptorSet* nextSet;
            unsigned slotOffset;
        } nested;
    };
} DescriptorSetSlot;

typedef struct _BindPoint
{
    uint32_t dirtyFlags;
    GrPipeline* grPipeline;
    GrDescriptorSet* grDescriptorSet;
    unsigned slotOffset;
    DescriptorSetSlot dynamicMemoryView;
    VkDeviceSize dynamicOffset;
    VkDescriptorSet descriptorSets[MAX_STAGE_COUNT];
} BindPoint;

typedef struct _PipelineCreateInfo
{
    VkPipelineCreateFlags createFlags;
    unsigned stageCount;
    VkPipelineShaderStageCreateInfo stageCreateInfos[MAX_STAGE_COUNT];
    VkPrimitiveTopology topology;
    uint32_t patchControlPoints;
    bool depthClipEnable;
    bool alphaToCoverageEnable;
    bool logicOpEnable;
    VkLogicOp logicOp;
    VkColorComponentFlags colorWriteMasks[GR_MAX_COLOR_TARGETS];
} PipelineCreateInfo;

typedef struct _PipelineSlot
{
    VkPipeline pipeline;
    // TODO keep track of individual parameters to minimize pipeline count
    const GrColorBlendStateObject* grColorBlendState;
    const GrMsaaStateObject* grMsaaState;
    const GrRasterStateObject* grRasterState;
    unsigned colorFormatCount;
    VkFormat colorFormats[GR_MAX_COLOR_TARGETS];
    VkFormat depthStencilFormat;
} PipelineSlot;

// Base object
typedef struct _GrBaseObject {
    GrObjectType grObjType;
} GrBaseObject;

// Device-bound object
typedef struct _GrObject {
    GrObjectType grObjType;
    GrDevice* grDevice;
    GrGpuMemory* grGpuMemory;
} GrObject;

typedef struct _GrBorderColorPalette {
    GrObject grObj;
    unsigned size;
    float* data;
} GrBorderColorPalette;

typedef struct _GrColorBlendStateObject {
    GrObject grObj;
    VkPipelineColorBlendAttachmentState states[GR_MAX_COLOR_TARGETS];
    float blendConstants[4];
} GrColorBlendStateObject;

typedef struct _GrDepthStencilStateObject {
    GrObject grObj;
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

typedef struct _GrvkMemoryHeap {
    unsigned vkMemoryTypeIndex;
    unsigned vkMemoryHeapIndex;
    VkDeviceSize size;
    VkMemoryPropertyFlags vkPropertyFlags;
} GrvkMemoryHeap;
typedef struct _GrvkMemoryHeaps {
    GrvkMemoryHeap heaps[GR_MAX_MEMORY_HEAPS];
    unsigned heapCount;
} GrvkMemoryHeaps;

typedef struct _GrDevice {
    GrBaseObject grBaseObj;
    VULKAN_DEVICE vkd;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    GrQueue* grUniversalQueue;
    GrQueue* grComputeQueue;
    GrQueue* grDmaQueue;
    VkBuffer universalAtomicCounterBuffer;
    VkBuffer computeAtomicCounterBuffer;
    GrBorderColorPalette* grBorderColorPalette;
    GrvkMemoryHeaps heaps;
    bool virtualizeMemory;
    VmaAllocator allocator;
} GrDevice;

typedef struct _GrEvent {
    GrObject grObj;
    VkEvent event;
} GrEvent;

typedef struct _GrFence {
    GrObject grObj;
    VkFence fence;
    bool submitted;
} GrFence;

typedef struct _GrGpuMemory {
    GrObject grObj; // FIXME base object?
    VkDeviceMemory deviceMemory;
    VkDeviceSize deviceSize;
    VkBuffer buffer;
    int heapIndex;
    VmaAllocation allocation;
} GrGpuMemory;

typedef struct _GrImage {
    GrObject grObj;
    VkImage image;
    VkBuffer buffer;
    VkImageType imageType;
    VkExtent3D extent;
    unsigned mipLevels;
    unsigned arrayLayers;
    VkFormat format;
    VkImageUsageFlags usage;
    bool multiplyCubeLayers;
    bool isCube;
    VmaAllocation imageAllocation;
    VmaAllocation bufferAllocation;
} GrImage;

typedef struct _GrCmdBuffer {
    GrObject grObj;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkQueryPool timestampQueryPool;
    DescriptorSetSlot atomicCounterSlot;
    // Resource tracking
    unsigned descriptorPoolCount;
    VkDescriptorPool* descriptorPools;
    // NOTE: grCmdBufferResetState resets everything past that point
    bool isBuilding;
    bool isRendering;
    int descriptorPoolIndex;
    GrFence* submitFence;
    // Graphics and compute bind points
    BindPoint bindPoints[2];
    // Graphics dynamic state
    GrViewportStateObject* grViewportState;
    GrRasterStateObject* grRasterState;
    GrMsaaStateObject* grMsaaState;
    GrDepthStencilStateObject* grDepthStencilState;
    GrColorBlendStateObject* grColorBlendState;
    // Render pass
    unsigned colorAttachmentCount;
    VkRenderingAttachmentInfoKHR colorAttachments[GR_MAX_COLOR_TARGETS];
    VkFormat colorFormats[GR_MAX_COLOR_TARGETS];
    bool hasDepthStencil;
    VkRenderingAttachmentInfoKHR depthAttachment;
    VkRenderingAttachmentInfoKHR stencilAttachment;
    VkFormat depthStencilFormat;
    VkExtent3D minExtent;
    // Linear image tracking
    GrImage** linearImages;
    unsigned linearImageCount;
    unsigned linearImageCapacity;
} GrCmdBuffer;

typedef struct _GrDescriptorSet {
    GrObject grObj;
    unsigned slotCount;
    DescriptorSetSlot* slots;
    // Linear image tracking
    GrImage** linearImages;
    unsigned linearImageCount;
    unsigned linearImageCapacity;
} GrDescriptorSet;

typedef struct _GrDepthStencilView {
    GrObject grObj;
    VkImageView imageView;
    VkExtent3D extent;
    VkFormat format;
} GrDepthStencilView;

typedef struct _GrColorTargetView {
    GrObject grObj;
    GrImage *image;
    VkImageView imageView;
    VkExtent3D extent;
    VkFormat format;
} GrColorTargetView;

typedef struct _GrImageView {
    GrObject grObj;
    GrImage *image;
    VkImageView imageView;
    VkFormat format;
} GrImageView;

typedef struct _GrMsaaStateObject {
    GrObject grObj;
    VkSampleCountFlags sampleCountFlags;
    VkSampleMask sampleMask;
} GrMsaaStateObject;

typedef struct _GrPhysicalGpu {
    GrBaseObject grBaseObj;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties physicalDeviceProps;
} GrPhysicalGpu;

typedef struct _GrPipeline {
    GrObject grObj;
    PipelineCreateInfo* createInfo;
    unsigned pipelineSlotCount;
    PipelineSlot* pipelineSlots;
    SRWLOCK pipelineSlotsLock;
    VkPipelineLayout pipelineLayout;
    unsigned stageCount;
    VkDescriptorSetLayout descriptorSetLayouts[MAX_STAGE_COUNT];
    GR_PIPELINE_SHADER shaderInfos[MAX_STAGE_COUNT];
    unsigned dynamicOffsetCount;
} GrPipeline;

typedef struct _GrQueueSemaphore {
    GrObject grObj;
    VkSemaphore semaphore;
    unsigned currentCount;
} GrQueueSemaphore;

typedef struct _GrRasterStateObject {
    GrObject grObj;
    VkPolygonMode polygonMode;
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
    float depthBiasConstantFactor;
    float depthBiasClamp;
    float depthBiasSlopeFactor;
} GrRasterStateObject;

typedef struct _GrSampler {
    GrObject grObj;
    VkSampler sampler;
} GrSampler;

typedef struct _GrShader {
    GrObject grObj;
    VkShaderModule shaderModule;
    unsigned bindingCount;
    IlcBinding* bindings;
    unsigned inputCount;
    IlcInput* inputs;
    char* name;
} GrShader;

typedef struct _GrQueryPool {
    GrObject grObj;
    VkQueryPool queryPool;
    VkQueryType queryType;
} GrQueryPool;

typedef struct _GrvkPreparationCommandBuffer {
    VkCommandBuffer commandBuffer;
    VkFence fence;
} GrvkPreparationCommandBuffer;

typedef struct _GrQueue {
    GrObject grObj; // FIXME base object?
    VkQueue queue;
    SRWLOCK queueLock;
    uint32_t queueFamilyIndex;
    unsigned globalMemRefCount;
    GR_MEMORY_REF* globalMemRefs;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffers[IMAGE_PREP_CMD_BUFFER_COUNT];
    unsigned commandBufferIndex;
} GrQueue;

typedef struct _GrViewportStateObject {
    GrObject grObj;
    VkViewport* viewports;
    unsigned viewportCount;
    VkRect2D* scissors;
    unsigned scissorCount;
} GrViewportStateObject;

typedef struct _GrWsiWinDisplay {
    GrObject grObj;
    HMONITOR hMonitor;
} GrWsiWinDisplay;

void grCmdBufferEndRenderPass(
    GrCmdBuffer* grCmdBuffer);

void grCmdBufferResetState(
    GrCmdBuffer* grCmdBuffer);

unsigned grImageGetBufferOffset(
    VkExtent3D extent,
    VkFormat format,
    unsigned arraySlice,
    unsigned arraySize,
    unsigned mipLevel);

unsigned grImageGetBufferRowPitch(
    VkExtent3D extent,
    VkFormat format,
    unsigned mipLevel);

unsigned grImageGetBufferDepthPitch(
    VkExtent3D extent,
    VkFormat format,
    unsigned mipLevel);

void grGpuMemoryBindBuffer(
    GrGpuMemory* grGpuMemory);

VkPipeline grPipelineFindOrCreateVkPipeline(
    GrPipeline* grPipeline,
    const GrColorBlendStateObject* grColorBlendState,
    const GrMsaaStateObject* grMsaaState,
    const GrRasterStateObject* grRasterState,
    unsigned colorFormatCount,
    const VkFormat* colorFormats,
    VkFormat depthStencilFormat);

GrQueue* grQueueCreate(
    GrDevice* grDevice,
    uint32_t queueFamilyIndex,
    uint32_t queueIndex);

#endif // GR_OBJECT_H_

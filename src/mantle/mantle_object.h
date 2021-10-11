#ifndef GR_OBJECT_H_
#define GR_OBJECT_H_

#include <stdbool.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "vulkan_loader.h"
#include "mantle/mantle.h"
#include "amdilc.h"

#define MAX_STAGE_COUNT     5 // VS, HS, DS, GS, PS
#define MSAA_LEVEL_COUNT    5 // 1, 2, 4, 8, 16x

#define UNIVERSAL_ATOMIC_COUNTERS_COUNT (512)
#define COMPUTE_ATOMIC_COUNTERS_COUNT   (1024)

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
typedef struct _GrRasterStateObject GrRasterStateObject;
typedef struct _GrViewportStateObject GrViewportStateObject;

typedef struct _DescriptorSetSlot
{
    DescriptorSetSlotType type;
    union {
        struct {
            VkDescriptorImageInfo imageInfo;
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
    bool depthAttachmentEnable;
    bool depthClipEnable;
    bool alphaToCoverageEnable;
    bool logicOpEnable;
    VkLogicOp logicOp;
    VkColorComponentFlags colorWriteMasks[GR_MAX_COLOR_TARGETS];
} PipelineCreateInfo;

typedef struct _PipelineSlot
{
    VkPipeline pipeline;
    VkRenderPass renderPass;
    // TODO keep track of individual parameters to minimize pipeline count
    const GrColorBlendStateObject* grColorBlendState;
    const GrMsaaStateObject* grMsaaState;
    const GrRasterStateObject* grRasterState;
    VkFormat attachmentFormats[GR_MAX_COLOR_TARGETS + 1];
    unsigned attachmentCount;
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

typedef struct _GrCmdFramebufferState {
    VkRenderPass renderPass;
    VkFramebuffer framebuffer;
} GrCmdFramebufferState;

typedef struct _GrCmdBuffer {
    GrObject grObj;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkBuffer atomicCounterBuffer;
    bool isBuilding;
    // Graphics and compute bind points
    BindPoint bindPoints[2];
    // Graphics dynamic state
    GrViewportStateObject* grViewportState;
    GrRasterStateObject* grRasterState;
    GrMsaaStateObject* grMsaaState;
    GrDepthStencilStateObject* grDepthStencilState;
    GrColorBlendStateObject* grColorBlendState;
    // Render pass
    GrCmdFramebufferState framebufferState;
    unsigned attachmentCount;
    VkImageView attachments[GR_MAX_COLOR_TARGETS + 1]; // Extra depth target
    VkFormat attachmentFormats[GR_MAX_COLOR_TARGETS + 1];
    VkImageLayout attachmentLayouts[GR_MAX_COLOR_TARGETS + 1];
    VkExtent3D minExtent;
    bool hasActiveRenderPass;
    // Resource tracking
    unsigned descriptorPoolIndex;
    unsigned descriptorPoolCount;
    VkDescriptorPool* descriptorPools;
    unsigned framebufferCount;
    VkFramebuffer* framebuffers;
    GrFence* submitFence;
} GrCmdBuffer;

typedef struct _GrColorBlendStateObject {
    GrObject grObj;
    VkPipelineColorBlendAttachmentState states[GR_MAX_COLOR_TARGETS];
    float blendConstants[4];
} GrColorBlendStateObject;

typedef struct _GrColorTargetView {
    GrObject grObj;
    VkImageView imageView;
    VkFormat format;
    VkExtent3D extent;
} GrColorTargetView;

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

typedef struct _GrDepthStencilView {
    GrObject grObj;
    VkImageView imageView;
    VkFormat format;
    VkExtent3D extent;
} GrDepthStencilView;

typedef struct _GrDescriptorSet {
    GrObject grObj;
    unsigned slotCount;
    DescriptorSetSlot* slots;
} GrDescriptorSet;

typedef struct _GrRenderPassState {
    VkRenderPass renderPass;
    VkFormat attachmentFormats[GR_MAX_COLOR_TARGETS + 1];
    VkImageLayout attachmentLayouts[GR_MAX_COLOR_TARGETS + 1];
    bool attachmentUsed[GR_MAX_COLOR_TARGETS + 1];
    unsigned attachmentCount;
    VkSampleCountFlags sampleCountFlags;
} GrRenderPassState;

// TODO: implement hash map (just like in dxvk)
typedef struct _GrRenderPassPool {
    GrRenderPassState* renderPasses;
    unsigned renderPassCount;
    CRITICAL_SECTION renderPassMutex;
} GrRenderPassPool;

typedef struct _GrDevice {
    GrBaseObject grBaseObj;
    VULKAN_DEVICE vkd;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    unsigned universalQueueFamilyIndex;
    unsigned computeQueueFamilyIndex;
    unsigned dmaQueueFamilyIndex;
    CRITICAL_SECTION universalQueueMutex;
    CRITICAL_SECTION computeQueueMutex;
    CRITICAL_SECTION dmaQueueMutex;
    VkBuffer universalAtomicCounterBuffer;
    VkBuffer computeAtomicCounterBuffer;
    GrBorderColorPalette* grBorderColorPalette;
    GrRenderPassPool renderPassPool;
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
    unsigned boundObjectCount;
    GrObject** boundObjects;
    CRITICAL_SECTION boundObjectsMutex;
} GrGpuMemory;

typedef struct _GrImage {
    GrObject grObj;
    VkImage image;
    VkBuffer buffer;
    VkImageType imageType;
    VkExtent3D extent;
    unsigned arrayLayers;
    VkFormat format;
    VkImageUsageFlags usage;
    bool needInitialDataTransferState;
    bool isCube;
} GrImage;

typedef struct _GrImageView {
    GrObject grObj;
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
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties physicalDeviceProps;
} GrPhysicalGpu;

typedef struct _GrPipeline {
    GrObject grObj;
    PipelineCreateInfo* createInfo;
    unsigned pipelineSlotCount;
    PipelineSlot* pipelineSlots;
    CRITICAL_SECTION pipelineSlotsMutex;
    VkPipelineLayout pipelineLayout;
    unsigned stageCount;
    VkDescriptorSetLayout descriptorSetLayouts[MAX_STAGE_COUNT];
    GR_PIPELINE_SHADER shaderInfos[MAX_STAGE_COUNT];
    unsigned dynamicOffsetCount;
    VkShaderModule tessellationModule;
    VkShaderModule patchedGeometryModule;
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
    uint32_t* code;
    unsigned codeSize;
    unsigned bindingCount;
    IlcBinding* bindings;
    unsigned inputCount;
    IlcInput* inputs;
    unsigned outputCount;
    uint32_t* outputLocations;
    char* name;
} GrShader;

typedef struct _GrQueryPool {
    GrObject grObj;
    VkQueryPool queryPool;
    VkQueryType queryType;
} GrQueryPool;

typedef struct _GrQueue {
    GrObject grObj; // FIXME base object?
    VkQueue queue;
    unsigned queueFamilyIndex;
    unsigned globalMemRefCount;
    GR_MEMORY_REF* globalMemRefs;
    CRITICAL_SECTION* mutex;
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
} GrWsiWinDisplay;

void grCmdBufferEndRenderPass(
    GrCmdBuffer* grCmdBuffer);

void grCmdBufferResetState(
    GrCmdBuffer* grCmdBuffer,
    bool preserveDescriptorPools);

unsigned grDeviceGetQueueFamilyIndex(
    const GrDevice* grDevice,
    GR_QUEUE_TYPE queueType);

CRITICAL_SECTION* grDeviceGetQueueMutex(
    GrDevice* grDevice,
    GR_QUEUE_TYPE queueType);

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

VkPipeline grPipelineFindOrCreateVkPipeline(
    VkRenderPass renderPass,
    GrPipeline* grPipeline,
    const GrColorBlendStateObject* grColorBlendState,
    const GrMsaaStateObject* grMsaaState,
    const GrRasterStateObject* grRasterState,
    const VkFormat* attachmentFormats,
    unsigned attachmentCount);

#endif // GR_OBJECT_H_

#ifndef GR_OBJECT_H_
#define GR_OBJECT_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "vulkan_loader.h"
#include "mantle/mantle.h"
#include "amdilc.h"

#define MAX_STAGE_COUNT 5 // VS, HS, DS, GS, PS

#define GET_OBJ_TYPE(obj) \
    (((GrBaseObject*)(obj))->grObjType)

#define GET_OBJ_DEVICE(obj) \
    (((GrObject*)(obj))->grDevice)

typedef enum _GrObjectType {
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
    GR_OBJ_TYPE_QUEUE_SEMAPHORE,
    GR_OBJ_TYPE_RASTER_STATE_OBJECT,
    GR_OBJ_TYPE_SAMPLER,
    GR_OBJ_TYPE_SHADER,
    GR_OBJ_TYPE_QUEUE,
    GR_OBJ_TYPE_VIEWPORT_STATE_OBJECT,
    GR_OBJ_TYPE_WSI_WIN_DISPLAY,
} GrObjectType;

typedef struct _GrColorBlendStateObject GrColorBlendStateObject;
typedef struct _GrDepthStencilStateObject GrDepthStencilStateObject;
typedef struct _GrDescriptorSet GrDescriptorSet;
typedef struct _GrDevice GrDevice;
typedef struct _GrFence GrFence;
typedef struct _GrPipeline GrPipeline;
typedef struct _GrRasterStateObject GrRasterStateObject;
typedef struct _GrViewportStateObject GrViewportStateObject;

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
    const GrColorBlendStateObject* grColorBlendState;
} PipelineSlot;

// Base object
typedef struct _GrBaseObject {
    GrObjectType grObjType;
} GrBaseObject;

// Device-bound object
typedef struct _GrObject {
    GrObjectType grObjType;
    GrDevice* grDevice;
} GrObject;

typedef struct _GrCmdBuffer {
    GrObject grObj;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    unsigned dirtyFlags;
    bool isBuilding;
    // Graphics and compute bind points
    struct {
        GrPipeline* grPipeline;
        GrDescriptorSet* descriptorSets[2];
        unsigned descriptorSetOffsets[2];
        GR_MEMORY_VIEW_ATTACH_INFO memoryBufferInfo;
    } bindPoint[2];
    // Graphics dynamic state
    GrViewportStateObject* grViewportState;
    GrRasterStateObject* grRasterState;
    GrDepthStencilStateObject* grDepthStencilState;
    GrColorBlendStateObject* grColorBlendState;
    // Render pass
    VkFramebuffer framebuffer;
    unsigned attachmentCount;
    VkImageView attachments[GR_MAX_COLOR_TARGETS + 1]; // Extra depth target
    VkExtent3D minExtent;
    bool hasActiveRenderPass;
    // Resource tracking
    unsigned descriptorPoolCount;
    VkDescriptorPool* descriptorPools;
    unsigned framebufferCount;
    VkFramebuffer* framebuffers;
    unsigned bufferViewCount;
    VkBufferView* bufferViews;
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
    VkExtent3D extent;
} GrDepthStencilView;

typedef enum _DescriptorSetSlotType
{
    SLOT_TYPE_NONE = 0,
    SLOT_TYPE_IMAGE_VIEW = 1,
    SLOT_TYPE_MEMORY_VIEW = 2,
    SLOT_TYPE_SAMPLER = 3,
    SLOT_TYPE_NESTED = 4,
} DescriptorSetSlotType;

typedef struct _DescriptorSetSlot
{
    DescriptorSetSlotType type;
    unsigned realDescriptorIndex;
    union {
        VkSampler sampler;
        VkImageView imageView;
        VkDeviceAddress nestedDescriptorSet;
    };
    //better separate this as it is owned by the slot
    VkImageLayout imageLayout;
    VkBufferView bufferView;
    VkBufferUsageFlags bufferUsage;
    VkImageUsageFlags imageUsage;
    VkBufferViewCreateInfo bufferViewCreateInfo;
} DescriptorSetSlot;

typedef struct _GrDescriptorSet {
    GrObject grObj;
    DescriptorSetSlot* slots;
    DescriptorSetSlot* tempSlots;
    unsigned slotCount;
    VkBuffer virtualDescriptorSet;
    VkDeviceMemory boundMemory;//TODO: make some buddy allocator or like that
    VkDeviceAddress bufferDevicePtr;
    void* boundMemoryHostPtr;
} GrDescriptorSet;

typedef struct _GrGlobalDescriptorSet {
    VkDescriptorSetLayout descriptorTableLayout;
    VkDescriptorSetLayout graphicsDynamicMemoryLayout;
    VkDescriptorSetLayout computeDynamicMemoryLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorTable;
    bool* samplers;
    bool* samplerPtr;
    bool* bufferViews;
    bool* bufferViewPtr;
    bool* images;
    bool* imagePtr;
    unsigned descriptorCount;
} GrGlobalDescriptorSet;

typedef struct _GrGlobalPipelineLayouts {
    VkPipelineLayout graphicsPipelineLayout;
    VkPipelineLayout computePipelineLayout;
} GrGlobalPipelineLayouts;

typedef struct _GrDevice {
    GrBaseObject grBaseObj;
    VULKAN_DEVICE vkd;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    unsigned universalQueueIndex;
    unsigned computeQueueIndex;
    GrGlobalDescriptorSet globalDescriptorSet;
    GrGlobalPipelineLayouts pipelineLayouts;
    unsigned vDescriptorSetMemoryTypeIndex;
    bool pushDescriptorSetSupported;// TODO: move this in separate struct
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
    VkImageUsageFlags usage;
} GrImageView;

typedef struct _GrMsaaStateObject {
    GrObject grObj;
} GrMsaaStateObject;

typedef struct _GrPhysicalGpu {
    GrBaseObject grBaseObj;
    VkPhysicalDevice physicalDevice;
} GrPhysicalGpu;

typedef struct _GrPipeline {
    GrObject grObj;
    PipelineCreateInfo* createInfo;
    unsigned pipelineSlotCount;
    PipelineSlot* pipelineSlots;
    CRITICAL_SECTION pipelineSlotsMutex;
    VkPipelineLayout pipelineLayout;
    VkRenderPass renderPass;
} GrPipeline;

typedef struct _GrQueueSemaphore {
    GrObject grObj;
    VkSemaphore semaphore;
} GrQueueSemaphore;

typedef struct _GrRasterStateObject {
    GrObject grObj;
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
    uint32_t* code;
    uint32_t  codeSize;
} GrShader;

typedef struct _GrQueue {
    GrObject grObj; // FIXME base object?
    VkQueue queue;
    unsigned queueIndex;
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
    const GrColorBlendStateObject* grColorBlendState);

#endif // GR_OBJECT_H_

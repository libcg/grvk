#ifndef MANTLE_EXT_H_
#define MANTLE_EXT_H_

#include "mantle.h"

#ifdef __cplusplus
extern "C" {
#endif

// Types

MANTLE_HANDLE(GR_BORDER_COLOR_PALETTE);

// Constants

#define GR_MAX_MSAA_RASTERIZER_SAMPLES 16 // FIXME

// Enumerations

typedef enum _GR_EXT_INFO_TYPE
{
    GR_EXT_INFO_TYPE_PHYSICAL_GPU_SUPPORTED_AXL_VERSION = 0x00306100,
    GR_EXT_INFO_TYPE_QUEUE_BORDER_COLOR_PALETTE_PROPERTIES = 0x00306800,
    GR_EXT_INFO_TYPE_QUEUE_CONTROL_FLOW_PROPERTIES = 0x00306801,
} GR_EXT_INFO_TYPE;

typedef enum _GR_EXT_BORDER_COLOR_TYPE
{
    GR_EXT_BORDER_COLOR_TYPE_PALETTE_ENTRY_0 = 0x0030a000,
} GR_EXT_BORDER_COLOR_TYPE;

typedef enum _GR_EXT_IMAGE_STATE
{
    GR_EXT_IMAGE_STATE_GRAPHICS_SHADER_FMASK_LOOKUP = 0x00300100,
    GR_EXT_IMAGE_STATE_COMPUTE_SHADER_FMASK_LOOKUP = 0x00300101,
    GR_EXT_IMAGE_STATE_DATA_TRANSFER_DMA_QUEUE = 0x00300102,
} GR_EXT_IMAGE_STATE;

typedef enum _GR_EXT_MEMORY_STATE
{
    GR_EXT_MEMORY_STATE_COPY_OCCLUSION_DATA = 0x00300000,
    GR_EXT_MEMORY_STATE_CMD_CONTROL = 0x00300001,
} GR_EXT_MEMORY_STATE;

typedef enum _GR_EXT_OCCLUSION_CONDITION
{
    GR_EXT_OCCLUSION_CONDITION_VISIBLE = 0x00300300,
    GR_EXT_OCCLUSION_CONDITION_INVISIBLE = 0x00300301,
} GR_EXT_OCCLUSION_CONDITION;

typedef enum _GR_EXT_CONTROL_FLOW_FEATURE_FLAGS
{
    GR_EXT_CONTROL_FLOW_OCCLUSION_PREDICATION = 0x00000001,
    GR_EXT_CONTROL_FLOW_MEMORY_PREDICATION = 0x00000002,
    GR_EXT_CONTROL_FLOW_CONDITIONAL_EXECUTION = 0x00000004,
    GR_EXT_CONTROL_FLOW_LOOP_EXECUTION = 0x00000008,
} GR_EXT_CONTROL_FLOW_FEATURE_FLAGS;

typedef enum _GR_EXT_QUEUE_TYPE
{
    GR_EXT_QUEUE_DMA = 0x00300200,
    GR_EXT_QUEUE_TIMER = 0x00300201,
} GR_EXT_QUEUE_TYPE;

typedef enum _GR_EXT_ACCESS_CLIENT
{
    GR_EXT_ACCESS_DEFAULT = 0x00000000,
    GR_EXT_ACCESS_CPU = 0x01000000,
    GR_EXT_ACCESS_UNIVERSAL_QUEUE = 0x02000000,
    GR_EXT_ACCESS_COMPUTE_QUEUE = 0x04000000,
    GR_EXT_ACCESS_DMA_QUEUE = 0x08000000,
} GR_EXT_ACCESS_CLIENT;

// Data Structures

typedef struct _GR_PHYSICAL_GPU_SUPPORTED_AXL_VERSION
{
    GR_UINT32 minVersion;
    GR_UINT32 maxVersion;
} GR_PHYSICAL_GPU_SUPPORTED_AXL_VERSION;

typedef struct _GR_BORDER_COLOR_PALETTE_PROPERTIES
{
    GR_UINT maxPaletteSize;
} GR_BORDER_COLOR_PALETTE_PROPERTIES;

typedef struct _GR_BORDER_COLOR_PALETTE_CREATE_INFO
{
    GR_UINT paletteSize;
} GR_BORDER_COLOR_PALETTE_CREATE_INFO;

typedef struct _GR_MSAA_QUAD_SAMPLE_PATTERN
{
    GR_OFFSET2D topLeft[GR_MAX_MSAA_RASTERIZER_SAMPLES];
    GR_OFFSET2D topRight[GR_MAX_MSAA_RASTERIZER_SAMPLES];
    GR_OFFSET2D bottomLeft[GR_MAX_MSAA_RASTERIZER_SAMPLES];
    GR_OFFSET2D bottomRight[GR_MAX_MSAA_RASTERIZER_SAMPLES];
} GR_MSAA_QUAD_SAMPLE_PATTERN;

typedef struct _GR_ADVANCED_MSAA_STATE_CREATE_INFO
{
    GR_UINT coverageSamples;
    GR_UINT pixelShaderSamples;
    GR_UINT depthStencilSamples;
    GR_UINT colorTargetSamples;
    GR_SAMPLE_MASK sampleMask;
    GR_UINT sampleClusters;
    GR_UINT alphaToCoverageSamples;
    GR_BOOL disableAlphaToCoverageDither;
    GR_BOOL customSamplePatternEnable;
    GR_MSAA_QUAD_SAMPLE_PATTERN customSamplePattern;
} GR_ADVANCED_MSAA_STATE_CREATE_INFO;

typedef struct _GR_FMASK_IMAGE_VIEW_CREATE_INFO
{
    GR_IMAGE image;
    GR_UINT baseArraySlice;
    GR_UINT arraySize;
} GR_FMASK_IMAGE_VIEW_CREATE_INFO;

typedef struct _GR_QUEUE_CONTROL_FLOW_PROPERTIES
{
    GR_UINT maxNestingLimit;
    GR_FLAGS controlFlowOperations;
} GR_QUEUE_CONTROL_FLOW_PROPERTIES;

typedef struct _GR_GPU_TIMESTAMP_CALIBRATION
{
    GR_UINT64 gpuTimestamp;
    union
    {
        GR_UINT64 cpuWinPerfCounter;
        GR_BYTE _padding[16];
    };
} GR_GPU_TIMESTAMP_CALIBRATION;

// Library Versioning Functions

GR_UINT32 grGetExtensionLibraryVersion();

// Border Color Palette Extension Functions

GR_RESULT GR_STDCALL grCreateBorderColorPalette(
    GR_DEVICE device,
    const GR_BORDER_COLOR_PALETTE_CREATE_INFO* pCreateInfo,
    GR_BORDER_COLOR_PALETTE* pPalette);

GR_RESULT GR_STDCALL grUpdateBorderColorPalette(
    GR_BORDER_COLOR_PALETTE palette,
    GR_UINT firstEntry,
    GR_UINT entryCount,
    const GR_FLOAT* pEntries);

GR_VOID grCmdBindBorderColorPalette(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_BORDER_COLOR_PALETTE palette);

// Advanced Multisampling Extnension Functions

GR_RESULT GR_STDCALL grCreateAdvancedMsaaState(
    GR_DEVICE device,
    const GR_ADVANCED_MSAA_STATE_CREATE_INFO* pCreateInfo,
    GR_MSAA_STATE_OBJECT* pState);

GR_RESULT GR_STDCALL grCreateFmaskImageView(
    GR_DEVICE device,
    const GR_FMASK_IMAGE_VIEW_CREATE_INFO* pCreateInfo,
    GR_IMAGE_VIEW* pView);

// Copy Occlusion Query Data Extension Functions

GR_RESULT GR_STDCALL grCmdCopyOcclusionData(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT startQuery,
    GR_UINT queryCount,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_BOOL accumulateData);

// Command Buffer Control Flow Extension Functions

GR_VOID grCmdSetOcclusionPredication(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT slot,
    GR_ENUM condition,
    GR_BOOL waitResults,
    GR_BOOL accumulateData);

GR_VOID grCmdResetOcclusionPredication(
    GR_CMD_BUFFER cmdBuffer);

GR_VOID grCmdSetMemoryPredication(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset);

GR_VOID grCmdResetMemoryPredication(
    GR_CMD_BUFFER cmdBuffer);

GR_VOID grCmdIf(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_GPU_SIZE srcOffset,
    GR_UINT64 data,
    GR_UINT64 mask,
    GR_ENUM func);

GR_VOID grCmdElse(
    GR_CMD_BUFFER cmdBuffer);

GR_VOID grCmdEndIf(
    GR_CMD_BUFFER cmdBuffer);

GR_VOID grCmdWhile(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_GPU_SIZE srcOffset,
    GR_UINT64 data,
    GR_UINT64 mask,
    GR_ENUM func);

GR_VOID grCmdEndWhile(
    GR_CMD_BUFFER cmdBuffer);

// Timer Queue Extension Functions

GR_RESULT GR_STDCALL grQueueDelay(
    GR_QUEUE queue,
    GR_FLOAT delay);

GR_RESULT GR_STDCALL grCalibrateGpuTimestamp(
    GR_DEVICE device,
    GR_GPU_TIMESTAMP_CALIBRATION* pCalibrationData);

// Undocumented Functions

GR_RESULT GR_STDCALL grAddEmulatedPrivateDisplay();

GR_RESULT GR_STDCALL grAddPerfExperimentCounter();

GR_RESULT GR_STDCALL grAddPerfExperimentTrace();

GR_RESULT GR_STDCALL grBlankPrivateDisplay();

GR_RESULT GR_STDCALL grCmdBeginPerfExperiment();

GR_RESULT GR_STDCALL grCmdBindUserData();

GR_RESULT GR_STDCALL grCmdCopyRegisterToMemory();

GR_RESULT GR_STDCALL grCmdDispatchOffset();

GR_RESULT GR_STDCALL grCmdDispatchOffsetIndirect();

GR_RESULT GR_STDCALL grCmdEndPerfExperiment();

GR_RESULT GR_STDCALL grCmdInsertTraceMarker();

GR_RESULT GR_STDCALL grCmdWaitMemoryValue();

GR_RESULT GR_STDCALL grCmdWaitRegisterValue();

GR_RESULT GR_STDCALL grCreatePerfExperiment();

GR_RESULT GR_STDCALL grCreatePrivateDisplayImage();

GR_RESULT GR_STDCALL grCreateVirtualDisplay();

GR_RESULT GR_STDCALL grDestroyVirtualDisplay();

GR_RESULT GR_STDCALL grDisablePrivateDisplay();

GR_RESULT GR_STDCALL grEnablePrivateDisplay();

GR_RESULT GR_STDCALL grEnablePrivateDisplayAudio();

GR_RESULT GR_STDCALL grFinalizePerfExperiment();

GR_RESULT GR_STDCALL grGetPrivateDisplayScanLine();

GR_RESULT GR_STDCALL grGetPrivateDisplays();

GR_RESULT GR_STDCALL grGetVirtualDisplayProperties();

GR_RESULT GR_STDCALL grOpenExternalSharedPrivateDisplayImage();

GR_RESULT GR_STDCALL grPrivateDisplayPresent();

GR_RESULT GR_STDCALL grQueueDelayAfterVsync();

GR_RESULT GR_STDCALL grQueueMigrateObjects();

GR_RESULT GR_STDCALL grQueueSetExecutionPriority();

GR_RESULT GR_STDCALL grRegisterPowerEvent();

GR_RESULT GR_STDCALL grRegisterPrivateDisplayEvent();

GR_RESULT GR_STDCALL grRemoveEmulatedPrivateDisplay();

GR_RESULT GR_STDCALL grSetEventAfterVsync();

GR_RESULT GR_STDCALL grSetPowerDefaultPerformance();

GR_RESULT GR_STDCALL grSetPowerProfile();

GR_RESULT GR_STDCALL grSetPowerRegions();

GR_RESULT GR_STDCALL grSetPrivateDisplayPowerMode();

GR_RESULT GR_STDCALL grSetPrivateDisplaySettings();

GR_RESULT GR_STDCALL grWinAllocMemory();

GR_RESULT GR_STDCALL grWinOpenExternalSharedImage();

GR_RESULT GR_STDCALL grWinOpenExternalSharedMemory();

GR_RESULT GR_STDCALL grWinOpenExternalSharedQueueSemaphore();

#ifdef __cplusplus
}
#endif

#endif // MANTLE_EXT_H_

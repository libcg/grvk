#ifndef AMDILC_H_
#define AMDILC_H_

#include <stdio.h>
#include <stdint.h>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#define ATOMIC_COUNTER_SET_ID       (1)
#define DYNAMIC_MEMORY_VIEW_BINDING_ID (0)
#define DYNAMIC_MEMORY_VIEW_DESCRIPTOR_SET_ID (0)
#define DESCRIPTOR_SET_ID           (2)

#define ILC_MAX_STRIDE_CONSTANTS    (8)

#define DESCRIPTOR_CONST_OFFSETS_OFFSET (sizeof(uint32_t) * ILC_MAX_STRIDE_CONSTANTS)
#define DESCRIPTOR_OFFSET_COUNT (32)

typedef enum _IlcBindingType {
    ILC_BINDING_SAMPLER,
    ILC_BINDING_RESOURCE,
} IlcBindingType;

typedef struct _IlcBinding {
    IlcBindingType type;
    uint32_t id;
    uint32_t offsetSpecId;
    uint32_t descriptorSetIndexSpecId;
    uint32_t ilIndex;
    uint32_t vkIndex; // Unique across shader stages
    VkDescriptorType descriptorType;
    int strideIndex; // Stride location in push constants (<0 means non-existent)
} IlcBinding;

typedef struct _IlcInput {
    uint32_t locationIndex;
    uint8_t interpMode;
} IlcInput;

typedef struct _IlcShader {
    unsigned codeSize;
    uint32_t* code;
    unsigned bindingCount;
    IlcBinding* bindings;
    unsigned inputCount;
    IlcInput* inputs;
    char* name;
} IlcShader;

typedef struct _IlcBindingPatchEntry {
    uint32_t id;
    uint32_t bindingIndex;
    uint32_t descriptorSetIndex;
} IlcBindingPatchEntry;

IlcShader ilcCompileShader(
    const void* code,
    unsigned size);

IlcShader ilcCompileRectangleGeometryShader(
    unsigned psInputCount,
    const IlcInput* psInputs);

void ilcDisassembleShader(
    FILE* file,
    const void* code,
    unsigned size);

void patchShaderBindings(
    void* code,
    uint32_t codeSize,
    const IlcBindingPatchEntry* entries,
    uint32_t entryCount);

#endif // AMDILC_H_

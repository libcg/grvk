#ifndef AMDILC_H_
#define AMDILC_H_

#include <stdio.h>
#include <stdint.h>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#define DESCRIPTOR_SET_ID           (0)
#define ATOMIC_COUNTER_SET_ID       (1)

#define ILC_MAX_STRIDE_CONSTANTS    (8)
#define ILC_MAX_OFFSET_CONSTANTS    (8)

typedef enum _IlcBindingType {
    ILC_BINDING_SAMPLER,
    ILC_BINDING_RESOURCE,
    ILC_BINDING_OFFSET_BUFFER,
} IlcBindingType;

typedef struct _IlcBinding {
    IlcBindingType type;
    uint32_t ilIndex;
    uint32_t vkIndex; // Unique across shader stages
    VkDescriptorType descriptorType;
    int strideIndex; // Stride location in push constants (<0 means non-existent)
    int offsetIndex; // Offset index for texel buffers
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

#endif // AMDILC_H_

#ifndef AMDILC_H_
#define AMDILC_H_

#include <stdio.h>
#include <stdint.h>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#define DESCRIPTOR_SET_ID           (0)
#define ATOMIC_COUNTER_SET_ID       (1)

#define ILC_MAX_STRIDE_CONSTANTS    (8)

typedef enum _IlcBindingType {
    ILC_BINDING_SAMPLER,
    ILC_BINDING_RESOURCE,
} IlcBindingType;

typedef struct _IlcBinding {
    IlcBindingType type;
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
    unsigned outputCount;
    uint32_t* outputLocations;
    char* name;
} IlcShader;

typedef struct _IlcRecompiledShader {
    unsigned codeSize;
    uint32_t* code;
} IlcRecompiledShader;

IlcShader ilcCompileShader(
    const void* code,
    unsigned size);

IlcRecompiledShader ilcRecompileShader(
    const void* code,
    unsigned size,
    const unsigned* inputPassthroughLocations,
    unsigned passthroughCount);

IlcShader ilcCompileRectangleGeometryShader(
    unsigned psInputCount,
    const IlcInput* psInputs);

void ilcDisassembleShader(
    FILE* file,
    const void* code,
    unsigned size);

#endif // AMDILC_H_

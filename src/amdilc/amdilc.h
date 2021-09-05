#ifndef AMDILC_H_
#define AMDILC_H_

#include <stdio.h>
#include <stdint.h>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

// TODO get rid of this
#define ILC_BASE_RESOURCE_ID    (16) // Samplers use 0-15

#define ILC_MAX_STRIDE_CONSTANTS    (8)

typedef struct _IlcBinding {
    uint32_t index;
    VkDescriptorType descriptorType;
    int strideIndex; // Stride location in push constants (<0 means non-existent)
} IlcBinding;

typedef struct _IlcShader {
    unsigned codeSize;
    uint32_t* code;
    unsigned bindingCount;
    IlcBinding* bindings;
    char* name;
} IlcShader;

IlcShader ilcCompileShader(
    const void* code,
    unsigned size);

void ilcDisassembleShader(
    FILE* file,
    const void* code,
    unsigned size);

#endif // AMDILC_H_

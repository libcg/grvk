#ifndef AMDILC_H_
#define AMDILC_H_

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <mantle/mantle.h>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#define ILC_BASE_RESOURCE_ID    (16) // Samplers use 0-15


enum IlcDescriptorResourceTable {
    TABLE_UNIFORM_TEXEL_BUFFER = 0,
    TABLE_STORAGE_TEXEL_BUFFER = 1,
    TABLE_STORAGE_BUFFER = 2,
    TABLE_STORAGE_IMAGE = 3,
    TABLE_SAMPLED_IMAGE = 4,
    TABLE_SAMPLER = 5,
    TABLE_MAX_ID  = 6,
};

typedef struct _IlcShader {
    unsigned codeSize;
    uint32_t* code;
} IlcShader;

IlcShader ilcCompileShader(
    const void* code,
    unsigned size,
    const GR_PIPELINE_SHADER* mappings);

void ilcDisassembleShader(
    FILE* file,
    const void* code,
    unsigned size);

#endif // AMDILC_H_

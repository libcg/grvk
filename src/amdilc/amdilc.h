#ifndef AMDILC_H_
#define AMDILC_H_

#include <stdio.h>
#include <stdint.h>

#define ILC_BASE_RESOURCE_ID    (16) // Samplers use 0-15

uint32_t* ilcCompileShader(
    unsigned* compiledSize,
    const void* code,
    unsigned size);

void ilcDisassembleShader(
    FILE* file,
    const void* code,
    unsigned size);

#endif // AMDILC_H_

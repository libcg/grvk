#ifndef AMDILC_H_
#define AMDILC_H_

#include <stdio.h>
#include <stdint.h>

uint32_t* ilcCompileShader(
    uint32_t* compiledSize,
    const void* code,
    unsigned size);

void ilcDisassembleShader(
    FILE* file,
    const void* code,
    unsigned size);

#endif // AMDILC_H_

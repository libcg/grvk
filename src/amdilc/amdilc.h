#ifndef AMDILC_H_
#define AMDILC_H_

#include <stdbool.h>
#include <stdint.h>

uint32_t* ilcCompileShader(
    uint32_t* compiledSize,
    const void* code,
    uint32_t size,
    bool dump);

#endif // AMDILC_H_

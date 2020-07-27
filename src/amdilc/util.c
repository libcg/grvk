#include <assert.h>
#include <amdilc_internal.h>

uint32_t getBits(
    const uint32_t dword,
    const uint32_t firstBit,
    const uint32_t lastBit)
{
    assert(firstBit >= 0 && lastBit < 32 && firstBit <= lastBit);

    return (dword >> firstBit) & (0xFFFFFFFF >> (32 - (lastBit - firstBit + 1)));
}

uint32_t getBit(
    const uint32_t dword,
    const uint32_t bit)
{
    return getBits(dword, bit, bit);
}

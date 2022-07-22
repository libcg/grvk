#include <stdio.h>
#include <windows.h>
#include <bcrypt.h>
#include "amdilc_internal.h"

#define SHA1_SIZE   (20)
#define NAME_LEN    (64)

static HCRYPTPROV mCryptProvider = 0;

static void freeSource(
    Source* src);

static void calcSha1(
    uint8_t* digest,
    const uint8_t* data,
    unsigned size)
{
    HCRYPTHASH hash;
    DWORD digestSize = 0;
    DWORD dwordSize = sizeof(DWORD);

    if (mCryptProvider == 0) {
        // This function is very slow (~250ms), acquire once
        CryptAcquireContext(&mCryptProvider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
    }

    CryptCreateHash(mCryptProvider, CALG_SHA1, 0, 0, &hash);
    CryptHashData(hash, data, size, 0);
    CryptGetHashParam(hash, HP_HASHSIZE, (BYTE*)&digestSize, &dwordSize, 0);
    assert(digestSize == SHA1_SIZE);
    CryptGetHashParam(hash, HP_HASHVAL, digest, &digestSize, 0);
    CryptDestroyHash(hash);
}

static void freeDestination(
    Destination* dst)
{
    if (dst->absoluteSrc != NULL) {
        freeSource(dst->absoluteSrc);
    }
    free(dst->absoluteSrc);
}

static void freeSource(
    Source* src)
{
    for (unsigned i = 0; i < src->srcCount; i++) {
        freeSource(&src->srcs[i]);
    }
    free(src->srcs);
}

static void freeInstruction(
    Instruction* instr)
{
    for (unsigned i = 0; i < instr->dstCount; i++) {
        freeDestination(&instr->dsts[i]);
    }
    for (unsigned i = 0; i < instr->srcCount; i++) {
        freeSource(&instr->srcs[i]);
    }
    free(instr->dsts);
    free(instr->srcs);
    free(instr->extras);
}

static void freeKernel(
    Kernel* kernel)
{
    for (unsigned i = 0; i < kernel->instrCount; i++) {
        freeInstruction(&kernel->instrs[i]);
    }
    free(kernel->instrs);
}

static bool isShaderDumpEnabled()
{
    const char* envValue = getenv("GRVK_DUMP_SHADERS");

    return envValue != NULL && strcmp(envValue, "1") == 0;
}

static void getShaderName(
    char* name,
    unsigned nameLen,
    const uint8_t* code,
    unsigned size)
{
    assert(size >= 2 * sizeof(Token));
    uint8_t shaderType = GET_BITS(((Token*)code)[1], 16, 23);
    uint8_t hash[SHA1_SIZE];

    calcSha1(hash, code, size);
    snprintf(name, nameLen,
             "%s_%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
             mIlShaderTypeNames[shaderType],
             hash[ 0], hash[ 1], hash[ 2], hash[ 3], hash[ 4],
             hash[ 5], hash[ 6], hash[ 7], hash[ 8], hash[ 9],
             hash[10], hash[11], hash[12], hash[13], hash[14],
             hash[15], hash[16], hash[17], hash[18], hash[19]);
}

static void dumpBuffer(
    const uint8_t* code,
    unsigned size,
    const char* name,
    const char* format)
{
    char fileName[NAME_LEN];
    snprintf(fileName, NAME_LEN, "%s_%s.bin", name, format);

    FILE* file = fopen(fileName, "wb");
    fwrite(code, 1, size, file);
    fclose(file);
}

static void dumpKernel(
    const Kernel* kernel,
    const char* name)
{
    char fileName[NAME_LEN];
    snprintf(fileName, NAME_LEN, "%s_il.txt", name);

    FILE* file = fopen(fileName, "w");
    ilcDumpKernel(file, kernel);
    fclose(file);
}

IlcShader ilcCompileShader(
    const void* code,
    unsigned size)
{
    char name[NAME_LEN];
    getShaderName(name, NAME_LEN, code, size);
    LOGV("compiling %s...\n", name);

    Kernel* kernel = ilcDecodeStream((Token*)code, size / sizeof(Token));
    bool dump = isShaderDumpEnabled();

    if (dump) {
        dumpBuffer(code, size, name, "il");
        dumpKernel(kernel, name);
    }

    IlcShader shader = ilcCompileKernel(kernel, name);

    if (dump) {
        dumpBuffer((uint8_t*)shader.code, shader.codeSize, name, "spv");
    }

    freeKernel(kernel);
    free(kernel);
    return shader;
}

void ilcDisassembleShader(
    FILE* file,
    const void* code,
    unsigned size)
{
    Kernel* kernel = ilcDecodeStream((Token*)code, size / sizeof(Token));

    ilcDumpKernel(file, kernel);
    freeKernel(kernel);
    free(kernel);
}

IlcRecompiledShader ilcRecompileShader(
    const void* code,
    unsigned size,
    const unsigned* inputPassthroughLocations,
    unsigned passthroughCount)
{
    return ilcRecompileKernel(code, size / sizeof(uint32_t), inputPassthroughLocations, passthroughCount);
}

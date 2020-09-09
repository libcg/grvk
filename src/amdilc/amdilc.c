#include <stdio.h>
#include <windows.h>
#include <bcrypt.h>
#include "amdilc_internal.h"

#define SHA1_SIZE   (20)
#define NAME_LEN    (128)

static void calcSha1(
    uint8_t* digest,
    const uint8_t* data,
    unsigned size)
{
    HCRYPTPROV provider;
    HCRYPTHASH hash;
    DWORD digestSize = 0;
    DWORD dwordSize = sizeof(DWORD);

    CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(provider, CALG_SHA1, 0, 0, &hash);
    CryptHashData(hash, data, size, 0);
    CryptGetHashParam(hash, HP_HASHSIZE, (BYTE*)&digestSize, &dwordSize, 0);
    assert(digestSize == SHA1_SIZE);
    CryptGetHashParam(hash, HP_HASHVAL, digest, &digestSize, 0);
    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
}

static void freeInstruction(
    Instruction* instr)
{
    free(instr->dsts);
    free(instr->srcs);
    free(instr->extras);
}

static void freeKernel(
    Kernel* kernel)
{
    for (int i = 0; i < kernel->instrCount; i++) {
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
    unsigned size,
    uint8_t shaderType)
{
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

static void dump(
    const uint8_t* buf,
    unsigned size,
    const char* fileName)
{
    FILE* f = fopen(fileName, "wb");
    fwrite(buf, 1, size, f);
    fclose(f);
}

static void dumpBuffer(
    const uint8_t* code,
    unsigned size,
    const char* name,
    const char* format)
{
    char fileName[NAME_LEN];
    snprintf(fileName, NAME_LEN, "%s_%s.bin", name, format);
    dump(code, size, fileName);
}

uint32_t* ilcCompileShader(
    uint32_t* compiledSize,
    const void* code,
    unsigned size)
{
    Kernel* kernel = ilcDecodeStream((Token*)code, size / sizeof(Token));
    bool dump = isShaderDumpEnabled();
    char name[NAME_LEN];

    if (dump) {
        getShaderName(name, NAME_LEN, code, size, kernel->shaderType);
        dumpBuffer(code, size, name, "il");

        // TODO dump to file
        ilcDumpKernel(kernel);
    }

    uint32_t* compiledCode = ilcCompileKernel(compiledSize, kernel);

    if (dump) {
        dumpBuffer((uint8_t*)compiledCode, *compiledSize, name, "spv");
    }

    freeKernel(kernel);
    free(kernel);
    return compiledCode;
}

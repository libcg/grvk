#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "amdilc.h"

int main(int argc, char *args[])
{
    if (argc < 3) {
        printf("usage: %s il.bin il.txt\n", args[0]);
        return 1;
    }

    FILE* inFile = fopen(args[1], "rb");
    assert(inFile != NULL);

    unsigned inSize;
    uint8_t* inBuf;
    fseek(inFile, 0, SEEK_END);
    inSize = ftell(inFile);
    inBuf = malloc(inSize);
    fseek(inFile, 0, SEEK_SET);
    fread(inBuf, 1, inSize, inFile);
    fclose(inFile);

    FILE* outFile = fopen(args[2], "wb");
    ilcDisassembleShader(outFile, inBuf, inSize);
    fclose(outFile);

    return 0;
}

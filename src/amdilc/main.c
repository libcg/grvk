#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "amdilc.h"
#include "logger.h"

int main(int argc, char* argv[])
{
    logInit("", "");

    if (argc < 2) {
        LOGE("%s [IL binary]\n", argv[0]);
        return 1;
    }

    FILE* file = fopen(argv[1], "rb");
    if (file == NULL) {
        LOGE("failed to open %s\n", argv[1]);
        return 1;
    }

    if (getenv("GRVK_DUMP_SHADERS") == NULL) {
        LOGW("GRVK_DUMP_SHADERS isn't set. Logs only.\n");
    }

    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);

    void* data = malloc(size);
    fread(data, 1, size, file);

    ilcCompileShader(data, size);

    free(data);
    fclose(file);

    return 0;
}

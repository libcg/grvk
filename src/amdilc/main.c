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
        LOGE("GRVK's amdilc -> SPIR-V offline compiler\n");
        LOGE("usage: GRVK_DUMP_SHADERS=1 %s [IL binary] ...\n", argv[0]);
        return 1;
    }

    if (getenv("GRVK_DUMP_SHADERS") == NULL || strcmp(getenv("GRVK_DUMP_SHADERS"), "1") != 0) {
        LOGW("GRVK_DUMP_SHADERS isn't set. Logs only.\n");
    }

    for (unsigned i = 1; i < argc; i++) {
        LOGW("compiling %s... (%d/%d)\n", argv[i], i, argc - 1);

        FILE* file = fopen(argv[i], "rb");
        if (file == NULL) {
            LOGE("failed to open %s\n", argv[i]);
            return 1;
        }

        fseek(file, 0, SEEK_END);
        int size = ftell(file);
        fseek(file, 0, SEEK_SET);

        void* data = malloc(size);
        fread(data, 1, size, file);
        fclose(file);

        ilcCompileShader(data, size);

        free(data);
    }

    return 0;
}

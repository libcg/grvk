#include "amdilc_spirv.h"
#include "amdilc_internal.h"

const IlcBindingPatchEntry* findEntryById(
    IlcSpvId id,
    const IlcBindingPatchEntry* entries,
    uint32_t entryCount)
{
    for (uint32_t i = 0; i < entryCount; i++) {
        if (id == entries[i].id) {
            return &entries[i];
        }
    }
    return NULL;
}

void patchShaderBindings(
    void* code,
    uint32_t codeSize,
    const IlcBindingPatchEntry* entries,
    uint32_t entryCount)
{
    uint32_t wordCount = codeSize / sizeof(uint32_t);
    IlcSpvWord* spirvWords = (IlcSpvWord*)code;
    for (uint32_t i = 5; i < wordCount;) {
        SpvOp opCode = spirvWords[i] & SpvOpCodeMask;
        unsigned instrWordCount = spirvWords[i] >> SpvWordCountShift;

        if (opCode == SpvOpDecorate) {
            IlcSpvWord id = spirvWords[i + 1];
            IlcSpvWord decoration = spirvWords[i + 2];
            if (decoration == SpvDecorationDescriptorSet || decoration == SpvDecorationBinding) {
                const IlcBindingPatchEntry* entry = findEntryById(id, entries, entryCount);
                if (entry != NULL) {
                    spirvWords[i + 3] = (decoration == SpvDecorationDescriptorSet) ? entry->descriptorSetIndex : entry->bindingIndex;                    
                }
            }
        }

        i += instrWordCount;
    }
}

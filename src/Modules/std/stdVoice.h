#ifndef _STD_VOICE_H
#define _STD_VOICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include "globals.h"

#include "Modules/rdroid/types.h"

void stdVoice_StartRecording();
void stdVoice_StopRecording();

// reads voice data to the input buffer, data may be compressed
int stdVoice_GetVoice(uint8_t* buffer, uint32_t bufferSize);

// decompress voice data
int stdVoice_Decompress(uint8_t* decompressed, uint32_t decompressedSize, const uint8_t* buffer, uint32_t bufferSize);

#ifdef __cplusplus
}
#endif


#endif // _STD_VOICE_H

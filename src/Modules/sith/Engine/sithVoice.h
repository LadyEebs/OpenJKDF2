#ifndef _SITH_VOICE_H
#define _SITH_VOICE_H

#ifdef PLATFORM_STEAM

#include "types.h"
#include "globals.h"

#include "Modules/rdroid/types.h"

int sithVoice_Open();
void sithVoice_Close();

int sithVoice_CreateChannel(DPID id);
void sithVoice_DeleteChannel(DPID id);
void sithVoice_AddVoicePacket(DPID id, const uint8_t* pVoiceData, size_t length);

void sithVoice_Tick();

void sithVoice_SetVolume(float volume);
#endif

#endif // _SITH_VOICE_H

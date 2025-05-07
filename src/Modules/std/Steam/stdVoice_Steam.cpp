#include "Modules/std/stdVoice.h"

#include "stdPlatform.h"
#include "jk.h"

#include <steam_api.h>

static int stdVoice_isRecording = 0;

void stdVoice_StartRecording()
{
	SteamUser()->StartVoiceRecording();
	stdVoice_isRecording = 1;
}

void stdVoice_StopRecording()
{
	SteamUser()->StopVoiceRecording();
	stdVoice_isRecording = 0;
}

// todo: reconcile SteamAPI_Init here and in stdComm_Steam
int stdVoice_GetVoice(uint8_t* buffer, size_t bufferSize)
{
	if (!SteamAPI_IsSteamRunning() || !stdVoice_isRecording)
		return 0;

	uint32 nBytesAvailable = 0;
	EVoiceResult res = SteamUser()->GetAvailableVoice(&nBytesAvailable, NULL, 0);
	if (res == k_EVoiceResultOK && nBytesAvailable > 0)
	{
		uint32 nBytesWritten = 0;
		res = SteamUser()->GetVoice(true, buffer, bufferSize, &nBytesWritten, false, NULL, 0, NULL, 0);
		if (res == k_EVoiceResultOK && nBytesWritten > 0)
			return nBytesWritten;
	}

	return 0;
}

int stdVoice_Decompress(uint8_t* decompressed, size_t decompressedSize, const uint8_t* buffer, size_t bufferSize)
{
	uint32 numUncompressedBytes = 0;
	EVoiceResult res = SteamUser()->DecompressVoice(buffer, bufferSize, decompressed, decompressedSize, &numUncompressedBytes, 11000);
	if (res == k_EVoiceResultOK && numUncompressedBytes > 0)
		return numUncompressedBytes;
		
	return 0;
}
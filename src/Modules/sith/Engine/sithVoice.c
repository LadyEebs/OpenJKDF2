#include "sithVoice.h"

#ifdef PLATFORM_STEAM

#include "stdPlatform.h"
#include "jk.h"
#include "Devices/sithControl.h"
#include "Devices/sithComm.h"

#include "Modules/std/stdVoice.h"

#include "Win95/stdSound.h"

int Main_bVerboseVoice = 0;

#define sithVoice_infoPrintf(fmt, ...) stdPlatform_Printf(fmt, ##__VA_ARGS__)
#define sithVoice_verbosePrintf(fmt, ...) if (Main_bVerboseVoice) \
    { \
        stdPlatform_Printf(fmt, ##__VA_ARGS__);  \
    } \
    ;

typedef struct sithVoicePacket
{
	uint32_t unSize;
	uint8_t* pData;
	struct sithVoicePacket* pNext;
} sithVoicePacket;

typedef struct sithVoiceChannel
{
	DPID dpId;
	sithVoicePacket* packets;
	stdSound_streamBuffer_t* stream;
} sithVoiceChannel;

sithVoiceChannel sithVoice_channels[32];
int sithVoice_activeChannels = 0;
int sithVoice_bIsOpen = 0;
int sithVoice_bIsRecording = 0;

uint8_t sithVoice_uncompressedVoice[VOICE_OUTPUT_SAMPLE_RATE * VOICE_OUTPUT_BYTES_PER_SAMPLE]; // too big for the stack
float sithVoice_volume = 1.0;

void sithVoice_SetVolume(float volume)
{
	sithVoice_volume = volume;
}

int sithVoice_CreateChannel(DPID id)
{
	if(sithVoice_activeChannels >= 32)
	{
		stdPrintf(pSithHS->errorPrint, ".\\Engine\\sithVoice.c", __LINE__, "Exceeded maximum number of voice channels.\n");
		return -1;
	}

	sithVoiceChannel* channel = &sithVoice_channels[sithVoice_activeChannels];
	channel->dpId = id;
	channel->stream = stdSound_StreamBufferCreate(0, VOICE_OUTPUT_SAMPLE_RATE, 16);
	channel->packets = NULL;

	return sithVoice_activeChannels++;
}

int sithVoice_GetChannel(DPID id)
{
	for (int i = 0; i < 32; ++i)
	{
		if (sithVoice_channels[i].dpId == id)
			return i;
	}
	return -1;
}


void sithVoice_DeleteChannelByIndex(int idx)
{
	sithVoice_channels[idx].dpId = 0;

	stdSound_StreamBufferRelease(sithVoice_channels[idx].stream);
	sithVoice_channels[idx].stream = NULL;

	sithVoicePacket* pVoicePacket = sithVoice_channels[idx].packets;
	while (pVoicePacket)
	{
		sithVoicePacket* pNextPacket = pVoicePacket->pNext;

		pSithHS->free(pVoicePacket->pData);
		pSithHS->free(pVoicePacket);

		pVoicePacket = pNextPacket;
	}
}

void sithVoice_DeleteChannel(DPID id)
{
	int idx = sithVoice_GetChannel(id);
	if (idx < 0)
		return;
	sithVoice_DeleteChannelByIndex(idx);
}

void sithVoice_AddVoicePacket(DPID id, const uint8_t* pVoiceData, size_t length)
{
	sithVoice_verbosePrintf("Decompressing %d bytes of voice data\n", length);

	int bytes = stdVoice_Decompress(sithVoice_uncompressedVoice, sizeof(sithVoice_uncompressedVoice), pVoiceData, length);
	if (!bytes)
		return;

	sithVoice_verbosePrintf("%d bytes of voice data decompressed\n", bytes);

	int idx = sithVoice_GetChannel(id);
	if (idx < 0)
		idx = sithVoice_CreateChannel(id);

	if (idx < 0)
	{
		stdPrintf(pSithHS->errorPrint, ".\\Engine\\sithVoice.c", __LINE__, "Failed to get or create voice channel for ID %ull.\n", id);
		return;
	}

	sithVoice_verbosePrintf("Adding a packet of %d bytes to voice channel %d\n", idx);

	sithVoicePacket* pVoicePacket = pSithHS->alloc(sizeof(sithVoicePacket));
	pVoicePacket->pData = pSithHS->alloc(bytes);
	memcpy(pVoicePacket->pData, sithVoice_uncompressedVoice, bytes);
	pVoicePacket->unSize = bytes;
	pVoicePacket->pNext = NULL;

	if (sithVoice_channels[idx].packets == NULL)
	{
		sithVoice_channels[idx].packets = pVoicePacket;
	}
	else
	{
		sithVoicePacket* pLastPacket = sithVoice_channels[idx].packets;
		while (pLastPacket->pNext)
			pLastPacket = pLastPacket->pNext;
		pLastPacket->pNext = pVoicePacket;
	}
}

int sithVoice_Open()
{
	memset(sithVoice_channels, 0, sizeof(sithVoice_channels));
	sithVoice_bIsOpen = 1;
	return 1;
}

void sithVoice_Close()
{
	sithVoice_bIsOpen = 0;

	for (int i = 0; i < sithVoice_activeChannels; ++i)
		sithVoice_DeleteChannelByIndex(i);
	sithVoice_activeChannels = 0;
}

void sithVoice_UpdateRecordingState()
{
	if(sithControl_ReadFunctionMap(INPUT_FUNC_VOICE, 0))
	{
		if (!sithVoice_bIsRecording)
		{
			sithVoice_bIsRecording = 1;
			stdVoice_StartRecording();
			sithVoice_verbosePrintf("Voice recording started\n");
		}

	}
	else if (sithVoice_bIsRecording)
	{
		stdVoice_StopRecording();
		sithVoice_bIsRecording = 0;
		sithVoice_verbosePrintf("Voice recording stopped\n");
	}
}

extern DPID stdComm_dplayIdSelf;

void sithVoice_CaptureAndSendVoice()
{
	uint8_t buffer[1024];
	int bytes = stdVoice_GetVoice(buffer, 1024);
	if (bytes && sithVoice_bIsRecording)
	{
		sithComm_SendVoice(buffer, bytes);

		//sithVoice_AddVoicePacket(stdComm_dplayIdSelf, buffer, bytes);
	}
}

void sithVoice_Playback()
{
	for (int i = 0; i < sithVoice_activeChannels; ++i)
	{
		sithVoiceChannel* channel = &sithVoice_channels[i];
		if (!channel->stream)
		{
			sithVoice_verbosePrintf("Voice channel stream for %ull is NULL\n", channel->dpId);
			continue;
		}

		int queued = stdSound_StreamBufferQueued(channel->stream);
		int processed = stdSound_StreamBufferProcessed(channel->stream);

		sithVoice_verbosePrintf("Voice channel %d: %d queued, %d processed\n", i, queued, processed);

		if ((queued == 4) && (processed == 0))
			continue;

		// proximity voice
		// todo: add a toggle so it can be set by the host
		if (channel->dpId != stdComm_dplayIdSelf)
		{
			int playerIdx = sithPlayer_ThingIdxToPlayerIdx(channel->dpId);
			if (playerIdx >= 0) // just in case
			{
				sithThing* pPlayerThing = jkPlayer_playerInfos[playerIdx].playerThing;

				// scale handled here for consistency with sithSoundMixer
				rdVector3 pos, vel;
				rdVector_Scale3(&pos, &pPlayerThing->position, 10.0);
				rdVector_Scale3(&vel, &pPlayerThing->physicsParams.vel, 10.0);

				stdSound_StreamBufferSetPosition(channel->stream, &pos);
				stdSound_StreamBufferSetVelocity(channel->stream, &vel);
			}
		}

		stdSound_StreamBufferUnqueue(channel->stream);

		int nMaxToQueue = 4 - queued + processed;

		int bDataQueued = 0;
		while (nMaxToQueue && channel->packets)
		{
			sithVoicePacket* packet = channel->packets;
			stdSound_StreamBufferQueue(channel->stream, packet->pData, packet->unSize);

			channel->packets = packet->pNext;
			pSithHS->free(packet->pData);
			pSithHS->free(packet);

			bDataQueued = 1;
			nMaxToQueue--;
		}

		if (bDataQueued && ((queued - processed) == 0))
		{
			stdSound_StreamBufferSetVolume(channel->stream, sithVoice_volume);
			stdSound_StreamBufferPlay(channel->stream);
		}
	}
}

void sithVoice_Tick()
{
	sithVoice_UpdateRecordingState();
	sithVoice_CaptureAndSendVoice();
	sithVoice_Playback();
}

#endif

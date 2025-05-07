#include "sithVoice.h"

#ifdef PLATFORM_STEAM

#include "stdPlatform.h"
#include "jk.h"
#include "Devices/sithControl.h"
#include "Devices/sithComm.h"

#include "Modules/std/stdVoice.h"

#include "Win95/stdSound.h"

typedef struct sithVoicePacket
{
	uint32_t unSize;
	void*    pData;
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

uint8_t sithVoice_uncompressedVoice[11000 * 2]; // too big for the stack
float sithVoice_volume = 1.0;

void sithVoice_SetVolume(float volume)
{
	sithVoice_volume = volume;
}

int sithVoice_CreateChannel(DPID id)
{
	if(sithVoice_activeChannels >= 32)
	{
		stdPlatform_Printf("Exceeded maximum number of voice channels\n");
		return -1;
	}

	sithVoiceChannel* channel = &sithVoice_channels[sithVoice_activeChannels];
	channel->dpId = id;
	channel->stream = stdSound_StreamBufferCreate(0, 11025, 16);

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

void sithVoice_AddVoicePacket(DPID id, const uint8_t* pVoiceData, size_t length)
{
	int bytes = stdVoice_Decompress(sithVoice_uncompressedVoice, sizeof(sithVoice_uncompressedVoice), pVoiceData, length);
	if (!bytes)
		return;
	
	int idx = sithVoice_GetChannel(id);
	if (idx < 0)
		idx = sithVoice_CreateChannel(id);

	if (idx < 0)
	{
		stdPlatform_Printf("Failed to get or create voice channel.\n");
		return;
	}

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
	stdVoice_StartRecording();
	sithVoice_bIsOpen = 1;
	return 1;
}

void sithVoice_Close()
{
	sithVoice_bIsOpen = 0;
	stdVoice_StopRecording();

	for (int i = 0; i < sithVoice_activeChannels; ++i)
	{
		sithVoice_channels[i].dpId = 0;

		stdSound_StreamBufferRelease(sithVoice_channels[i].stream);
		sithVoice_channels[i].stream = NULL;

		sithVoicePacket* pVoicePacket = sithVoice_channels[i].packets;
		while (pVoicePacket)
		{
			sithVoicePacket* pNextPacket = pVoicePacket->pNext;

			pSithHS->free(pVoicePacket->pData);
			pSithHS->free(pVoicePacket);

			pVoicePacket = pNextPacket;
		}
	}
	sithVoice_activeChannels = 0;
}

void sithVoice_Tick()
{
	int pushToTalk = 0;
	sithControl_ReadFunctionMap(INPUT_FUNC_VOICE, &pushToTalk);
	if (pushToTalk)
	{
		uint8_t buffer[1024];
		int bytes = stdVoice_GetVoice(buffer, 1024);
		if (bytes)
			sithComm_SendVoice(buffer, bytes);
	}

	for (int i = 0; i < sithVoice_activeChannels; ++i)
	{
		sithVoiceChannel* channel = &sithVoice_channels[sithVoice_activeChannels];

		int queued = stdSound_StreamBufferQueued(channel->stream);
		int processed = stdSound_StreamBufferProcessed(channel->stream);

		if ((queued == 4) && (processed == 0))
			continue;

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

#endif

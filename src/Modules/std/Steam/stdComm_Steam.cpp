#include "stdComm_Steam.h"

#include "Win95/stdComm.h"
#include "Dss/sithMulti.h"
#include "General/stdString.h"
#include "stdPlatform.h"
#include "jk.h"

extern "C"{
#include "Win95/stdDisplay.h"
#include "General/stdBitmap.h"
		// todo: make this a callback this shouldn't be here
	extern int jkGuiMultiplayer_Show2();

}

#include "SDL2_helper.h"

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <map>
#include <cctype>

#include <isteamnetworkingsockets.h>
#include <isteamnetworkingmessages.h>
#include <steamnetworkingtypes.h>
#include <isteamnetworkingutils.h>
#include <steam_api.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#if 1

static int GenerateChallenge(char* out, int length) {
	static const char charset[] =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	int i;

	if (length <= 1) return 0;

#ifdef _WIN32
	HCRYPTPROV hProv;
	if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		return 0;
	}

	if (!CryptGenRandom(hProv, length - 1, (BYTE*)out))
	{
		CryptReleaseContext(hProv, 0);
		return 0;
	}

	for (i = 0; i < length - 1; ++i)
	{
		out[i] = charset[((unsigned char)out[i]) % (sizeof(charset) - 1)];
	}

	CryptReleaseContext(hProv, 0);
#else
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0) return 0;

	if (read(fd, out, length - 1) != length - 1)
	{
		close(fd);
		return 0;
	}
	close(fd);

	for (i = 0; i < length - 1; ++i)
	{
		out[i] = charset[((unsigned char)out[i]) % (sizeof(charset) - 1)];
	}
#endif

	out[length - 1] = '\0';
	return 1;
}

extern "C" {
	void DirectPlayer_AddPlayer(CSteamID steamID);
}

static CSteamID stdComm_steamLobbyID;

struct LobbySystem
{
	LobbySystem()
		: dataUpdateCallback(this, &LobbySystem::OnLobbyDataUpdate)
		, chatUpdateCallback(this, &LobbySystem::OnLobbyChatUpdate)
		, inviteCallback(this, &LobbySystem::OnGameInvite)
		, sessionCallback(this, &LobbySystem::OnSessionRequest)
	{
	}

	int GetLobbySessionDesc(CSteamID lobbyID, stdCommSession* pEntry)
	{
		if (!lobbyID.IsValid())
			return 0;

		pEntry->guidInstance = lobbyID.ConvertToUint64();

		pEntry->maxPlayers = SteamMatchmaking()->GetLobbyMemberLimit(lobbyID);
		pEntry->numPlayers = SteamMatchmaking()->GetNumLobbyMembers(lobbyID);

		MultiByteToWideChar(CP_UTF8, 0, SteamMatchmaking()->GetLobbyData(lobbyID, "serverName"), 32, pEntry->serverName, 32);
		strcpy_s(pEntry->episodeGobName, 32, SteamMatchmaking()->GetLobbyData(lobbyID, "episodeGobName"));
		strcpy_s(pEntry->mapJklFname, 32, SteamMatchmaking()->GetLobbyData(lobbyID, "mapJklFname"));
		//wchar_t wPassword[32];
		pEntry->sessionFlags = std::stoul(SteamMatchmaking()->GetLobbyData(lobbyID, "sessionFlags"));
		pEntry->checksumSeed = std::stoi(SteamMatchmaking()->GetLobbyData(stdComm_steamLobbyID, "checksum")); // todo: encryption
		//int field_E0;
		pEntry->multiModeFlags = std::stoul(SteamMatchmaking()->GetLobbyData(lobbyID, "multiModeFlags"));
		pEntry->tickRateMs = std::stoi(SteamMatchmaking()->GetLobbyData(lobbyID, "tickRateMs"));
		pEntry->maxRank = std::stoi(SteamMatchmaking()->GetLobbyData(lobbyID, "maxRank"));

		pEntry->field_E0 = 10; // todo: fixme

		return 1;
	}

	uint32_t CreateLobby(stdCommSession* pEntry)
	{
		success = 0;

		SteamAPICall_t hSteamAPICall = SteamMatchmaking()->CreateLobby(pEntry->multiModeFlags & MULTIMODEFLAG_FRIENDS_ONLY ? k_ELobbyTypeFriendsOnly : k_ELobbyTypePublic, pEntry->maxPlayers);
		createResult.Set(hSteamAPICall, this, &LobbySystem::OnLobbyCreated);

		// wait for result
		while (success == 0)
			SteamAPI_RunCallbacks();

		return success;
	}

	uint32_t JoinLobby()
	{
		success = 0;

		SteamAPICall_t hSteamAPICall = SteamMatchmaking()->JoinLobby(stdComm_steamLobbyID);
		joinResult.Set(hSteamAPICall, this, &LobbySystem::OnLobbyEntered);

		// wait for join result
		while (success == 0)
			SteamAPI_RunCallbacks();

		return success;
	}

	void ListLobbies()
	{
		success = 0;

		SteamMatchmaking()->AddRequestLobbyListDistanceFilter(k_ELobbyDistanceFilterFar);
		SteamAPICall_t hSteamAPICall = SteamMatchmaking()->RequestLobbyList();
		listResult.Set(hSteamAPICall, this, &LobbySystem::OnLobbyMatchList);

		// wait for the result
		while (success == 0)
			SteamAPI_RunCallbacks();
	}

	void OnLobbyCreated(LobbyCreated_t* pCreated, bool)
	{
		success = pCreated->m_eResult;
		stdComm_steamLobbyID = pCreated->m_ulSteamIDLobby;
		
		char name[32];
		WideCharToMultiByte(CP_UTF8, 0, jkPlayer_playerShortName, 32, name, 32, nullptr, 0);
		SteamMatchmaking()->SetLobbyMemberData(stdComm_steamLobbyID, "nickname", name);
	}

	void OnLobbyEntered(LobbyEnter_t* pEntered, bool)
	{
		if (CSteamID(pEntered->m_ulSteamIDLobby) == stdComm_steamLobbyID)
		{
			char name[32];
			WideCharToMultiByte(CP_UTF8, 0, jkPlayer_playerShortName, 32, name, 32, nullptr, 0);

			SteamMatchmaking()->SetLobbyMemberData(stdComm_steamLobbyID, "nickname", name);
			success = pEntered->m_EChatRoomEnterResponse;
			DirectPlay_EnumPlayers(0);
		}
	}

	void OnLobbyMatchList(LobbyMatchList_t* pLobbyMatchList, bool bIOFailure)
	{
		if(jkGuiMultiplayer_aEntries)
		{
			std_pHS->free(jkGuiMultiplayer_aEntries);
			jkGuiMultiplayer_aEntries = 0;
		}
		dplay_dword_55D618 = max(pLobbyMatchList->m_nLobbiesMatching, 1);
		jkGuiMultiplayer_aEntries = (stdCommSession*)std_pHS->alloc(dplay_dword_55D618 * sizeof(stdCommSession));
		memset(jkGuiMultiplayer_aEntries, 0, dplay_dword_55D618 * sizeof(stdCommSession));

		for (int i = 0; i < pLobbyMatchList->m_nLobbiesMatching; ++i)
		{
			CSteamID lobbyID = SteamMatchmaking()->GetLobbyByIndex(i);
			CSteamID ownerID = SteamMatchmaking()->GetLobbyOwner(lobbyID);
			if (lobbyID.IsValid())// && ownerID.IsValid())
			{
				stdCommSession* pEntry = &jkGuiMultiplayer_aEntries[i];
				GetLobbySessionDesc(lobbyID, pEntry);
			}
		}

		success = 1;
	}

	void OnLobbyDataUpdate(LobbyDataUpdate_t* update)
	{
		//CSteamID lobbyID(update->m_ulSteamIDLobby);
		//if (lobbyID == stdComm_steamLobbyID)
		//{
		//	CSteamID ownerID = SteamMatchmaking()->GetLobbyOwner(lobbyID);
		//	if(ownerID == SteamUser()->GetSteamID())
		//		stdComm_bIsServer = 1;
		//	else
		//		stdComm_bIsServer = 0;
		//}
	}

	void OnLobbyChatUpdate(LobbyChatUpdate_t* update)
	{
		char nonce[16];
		switch(update->m_rgfChatMemberStateChange)
		{
		case k_EChatMemberStateChangeEntered:
			// todo: password
			//if (GenerateChallenge(nonce, sizeof(nonce)))
			//	//SteamMatchmaking()->SendLobbyChatMsg()
			break;
		case k_EChatMemberStateChangeLeft:
		case k_EChatMemberStateChangeDisconnected:
			// todo: migrate host
			break;
		case k_EChatMemberStateChangeKicked:
		case k_EChatMemberStateChangeBanned:
			sithMulti_SendQuit(update->m_ulSteamIDUserChanged);
			break;
		default:
			break;
		}
	}

	void OnGameInvite(GameRichPresenceJoinRequested_t* invite)
	{
		stdPlatform_Printf("Invited with connection string %s\n", invite->m_rgchConnect);

		//stdCommSession* pEntry = &jkGuiMultiplayer_aEntries[0];
		//pEntry->hostID = lobbyID.ConvertToUint64();
		//DirectPlay_IdkSessionDesc(pEntry);

		//stdComm_steamLobbyID.Clear();
		//stdComm_steamLobbyID = invite->;
		//if (stdComm_steamLobbyID.IsValid())
		//{
		//	stdPlatform_Printf("Connecting to lobby failed, lobby isn't valid\n");
		//	return;
		//}
		//
		//stdComm_dword_8321E8 = 0;
		//stdComm_dword_8321E0 = 1;
		//stdComm_dplayIdSelf = DirectPlay_CreatePlayer(jkPlayer_playerShortName, 0);
		//stdComm_bIsServer = 0;
		//
		//uint32_t result = JoinLobby();
		//if (result == k_EChatRoomEnterResponseSuccess)
		//{
		//	jkGuiMultiplayer_Show2();
		//	return;
		//}
		//
		//stdPlatform_Printf("Failed to join lobby with error %d\n", result);
	}

	void OnSessionRequest(SteamNetworkingMessagesSessionRequest_t* request)
	{
		// reject the incoming request if the user is blocked
		if (SteamFriends()->HasFriend(request->m_identityRemote.GetSteamID(), k_EFriendFlagBlocked))
			return;

		SteamNetworkingMessages()->AcceptSessionWithUser(request->m_identityRemote);
	}
	
	uint32_t success = 0;

	CCallback<LobbySystem, LobbyDataUpdate_t> dataUpdateCallback;
	CCallback<LobbySystem, LobbyChatUpdate_t> chatUpdateCallback;
	CCallback<LobbySystem, GameRichPresenceJoinRequested_t > inviteCallback;
	CCallback<LobbySystem, SteamNetworkingMessagesSessionRequest_t> sessionCallback;
	
	CCallResult<LobbySystem, LobbyCreated_t> createResult;
	CCallResult<LobbySystem, LobbyEnter_t> joinResult;
	CCallResult<LobbySystem, LobbyMatchList_t> listResult;
};

LobbySystem lobbyFuncs;

extern "C" {
	
extern int Main_bVerboseNetworking;

void DirectPlay_ClearFriends()
{
	if (DirectPlay_apFriends)
	{
		for (int i = 0; i < DirectPlay_numFriends; ++i)
		{
			if(DirectPlay_apFriends[i].thumbnail)
			{
				stdDisplay_VBufferFree(DirectPlay_apFriends[i].thumbnail);
				DirectPlay_apFriends[i].thumbnail=0;
			}
		}
			//stdBitmap_Free(DirectPlay_apFriends[i].thumbnail);

		pHS->free(DirectPlay_apFriends);
		DirectPlay_apFriends = 0;
	}
}

// pretty sure this stuff should be on JK game side
void stdComm_Steam_Startup()
{
	if (!SteamAPI_Init())
	{
		jkGuiMultiplayer_numConnections = 1;
		jk_snwprintf(jkGuiMultiplayer_aConnections[0].name, 0x80, L"Local Play");
		stdComm_dword_8321E0 = 0;

		//memset(jkGuiMultiplayer_aEntries, 0, sizeof(stdCommSession) * 32);
		jkGuiMultiplayer_aEntries = 0;
		dplay_dword_55D618 = 0;
		return;
	}
	
	//SteamUtils()->SetWarningMessageHook(&SteamWarningMessageHook);
	//SteamNetworkingUtils()->InitRelayNetworkAccess();

	jkGuiMultiplayer_numConnections = 1;
	jk_snwprintf(jkGuiMultiplayer_aConnections[0].name, 0x80, L"Steam Lobbies");
	stdComm_dword_8321E0 = 0;

	//memset(jkGuiMultiplayer_aEntries, 0, sizeof(stdCommSession) * 32);
	jkGuiMultiplayer_aEntries = 0;
	dplay_dword_55D618 = 0;
}

void stdComm_Steam_Shutdown()
{
	SteamAPI_Shutdown();
	if (jkGuiMultiplayer_aEntries)
	{
		std_pHS->free(jkGuiMultiplayer_aEntries);
		jkGuiMultiplayer_aEntries = 0;
	}
}

int DirectPlay_Receive(DPID* pIdOut, void* pMsgIdOut, int* pLenOut)
{
	SteamAPI_RunCallbacks();
	SteamNetworkingSockets()->RunCallbacks();

	SteamNetworkingMessage_t* pMsg = nullptr;
	int numMsgs = SteamNetworkingMessages()->ReceiveMessagesOnChannel(0, &pMsg, 1);
	if (numMsgs == 0)
		return -1;
	if (numMsgs < 0)
	{
		stdPlatform_Printf("Error checking for messages (%d)\n", numMsgs);
		return -1;
	}
	*pIdOut = pMsg->m_identityPeer.GetSteamID64();

	int maxLen = *pLenOut;
	if (maxLen > pMsg->m_cbSize)
		maxLen = pMsg->m_cbSize;

	memcpy(pMsgIdOut, pMsg->m_pData, maxLen);
	*pLenOut = maxLen;

	if (Main_bVerboseNetworking)
	{
		const char* name = SteamFriends()->GetFriendPersonaName(pMsg->m_identityPeer.GetSteamID());
		stdPlatform_Printf("Recv %x bytes from %s (%s, %x)\n", maxLen, name, sithDSS_IdToStr(*(uint32_t*)pMsgIdOut & 0xFF), *(uint32_t*)pMsgIdOut);
	}

	pMsg->Release();

	return 0;
}

BOOL DirectPlay_Send(DPID idFrom, DPID idTo, void* lpData, DWORD dwDataSize)
{
	SteamAPI_RunCallbacks();
	SteamNetworkingSockets()->RunCallbacks();

	SteamNetworkingIdentity id;
	if (idTo == 0)
		id.SetSteamID(SteamMatchmaking()->GetLobbyOwner(stdComm_steamLobbyID));
	else
		id.SetSteamID64(idTo);

	if (idFrom == idTo)
	{
		stdPlatform_Printf("Tried to send message to self\n");
		return 0;
	}

	EResult result = SteamNetworkingMessages()->SendMessageToUser(id, lpData, dwDataSize, k_nSteamNetworkingSend_Reliable, 0);
	if (Main_bVerboseNetworking)
	{
		const char* name = SteamFriends()->GetFriendPersonaName(id.GetSteamID());
		stdPlatform_Printf("Sent %x bytes to %s (%s, %x)\n", dwDataSize, name, sithDSS_IdToStr(*(uint32_t*)lpData & 0xFF), *(uint32_t*)lpData);
	}

	return (result == k_EResultOK);
}

int stdComm_OpenConnection(void* a)
{
	stdComm_dword_8321DC = 1;
	return 0;
}

void stdComm_CloseConnection()
{
	if (stdComm_dword_8321DC)
	{
		if (stdComm_dword_8321E0)
		{
			//DirectPlay_DestroyPlayer(stdComm_dplayIdSelf);
			DirectPlay_Close();
			stdComm_dword_8321E0 = 0;
			stdComm_bIsServer = 0;
			stdComm_dplayIdSelf = 0;
		}
		//DirectPlay_CloseConnection();
		stdComm_dword_8321DC = 0;
	}
}

int stdComm_Open(int idx, wchar_t* pwPassword)
{
	if (!SteamAPI_IsSteamRunning())
		return 0;

	jkGuiMultiplayer_checksumSeed = jkGuiMultiplayer_aEntries[idx].checksumSeed;

	stdComm_dword_8321E8 = 0;
	stdComm_dword_8321E0 = 1;
	stdComm_dplayIdSelf = DirectPlay_CreatePlayer(jkPlayer_playerShortName, 0);
	stdComm_bIsServer = 0;

	stdComm_steamLobbyID.Clear();
	stdComm_steamLobbyID.SetFromUint64(jkGuiMultiplayer_aEntries[idx].guidInstance);

	uint32_t result = lobbyFuncs.JoinLobby();
	if (result == k_EChatRoomEnterResponseSuccess)
		return 0;
	
	stdPlatform_Printf("Failed to join lobby with error %d\n", result);
	return result;
}

void stdComm_Close()
{
	stdComm_CloseConnection();
}

int DirectPlay_SendLobbyMessage(void* pPkt, uint32_t pktLen)
{
	SteamMatchmaking()->SendLobbyChatMsg(stdComm_steamLobbyID, pPkt, pktLen);

	return 1;
}

int DirectPlay_EnumSessions2()
{
	return 0;
}

void DirectPlay_SetSessionDesc(const char* a1, DWORD maxPlayers)
{
	SteamMatchmaking()->SetLobbyData(stdComm_steamLobbyID, "mapJklFname", a1);
	SteamMatchmaking()->SetLobbyMemberLimit(stdComm_steamLobbyID, maxPlayers);
}

BOOL DirectPlay_SetSessionFlagidk(int a1)
{
	return 1;
}

BOOL DirectPlay_Startup()
{
	jkGuiMultiplayer_numConnections = 0;
	memset(&jkGuiMultiplayer_aConnections, 0, 0x1180u);

	return 1;
}

// 0 = standard start, 1 = host, 2 = peer
int DirectPlay_EarlyInit(wchar_t* pwIdk, wchar_t* pwPlayerName)
{
	if (!SteamAPI_IsSteamRunning())
		return 0;

	// This can launch straight into a game? Gaming Zone stuff. 1 and 2 autolaunch an MP game.
	if (pwIdk && pwIdk[0] != L'\0')
	{
		if (Main_bVerboseNetworking)
			stdPlatform_Printf("Invited to game %ls\n", pwIdk);

		uint64_t id = std::wcstoull(pwIdk, 0, 0);
		stdComm_steamLobbyID.Clear();
		stdComm_steamLobbyID.SetFromUint64(id);
		if (!stdComm_steamLobbyID.IsValid())
		{
			stdPlatform_Printf("Lobby %ls is invalid\n", pwIdk);
			return 0;
		}

		stdComm_dword_8321E8 = 0;
		stdComm_dword_8321E0 = 1;
		stdComm_dplayIdSelf = DirectPlay_CreatePlayer(jkPlayer_playerShortName, 0);
		stdComm_bIsServer = 0;

		uint32_t result = lobbyFuncs.JoinLobby();
		if (result == k_EChatRoomEnterResponseSuccess)
			return 2; // peer

		stdPlatform_Printf("Failed to join %ls lobby with error %d\n", pwIdk, result);
		return 0;
	}

	return 0;
}


DPID DirectPlay_CreatePlayer(wchar_t* pwIdk, int idk2)
{
	return SteamUser()->GetSteamID().ConvertToUint64();
}

void DirectPlay_Close()
{
	SteamMatchmaking()->LeaveLobby(stdComm_steamLobbyID);
}

int DirectPlay_OpenHost(stdCommSession* pEntry)
{
	if (!SteamAPI_IsSteamRunning())
		return 0;

	uint32_t result = lobbyFuncs.CreateLobby(pEntry);
	if (result != k_EChatRoomEnterResponseSuccess)
	{
		stdPlatform_Printf("Failed to create lobby with error %d\n", result);
		stdComm_bIsServer = 0;
		return result;
	}

	// todo: should be in the session functions
	// make sure we're the owner
	SteamMatchmaking()->SetLobbyOwner(stdComm_steamLobbyID, SteamUser()->GetSteamID());

	// set the session description through lobby metadata
	char serverName[32];
	MultiByteToWideChar(CP_UTF8, 0, serverName, 32, pEntry->serverName, 32);

	SteamMatchmaking()->SetLobbyData(stdComm_steamLobbyID, "serverName", serverName);
	SteamMatchmaking()->SetLobbyData(stdComm_steamLobbyID, "episodeGobName", pEntry->episodeGobName);
	SteamMatchmaking()->SetLobbyData(stdComm_steamLobbyID, "mapJklFname", pEntry->mapJklFname);
	//wchar_t wPassword[32];
	SteamMatchmaking()->SetLobbyData(stdComm_steamLobbyID, "sessionFlags", std::to_string(pEntry->sessionFlags).c_str());
	SteamMatchmaking()->SetLobbyData(stdComm_steamLobbyID, "checksum", std::to_string(pEntry->checksumSeed).c_str()); // todo: encryption
	//int field_E0;
	SteamMatchmaking()->SetLobbyData(stdComm_steamLobbyID, "multiModeFlags", std::to_string(pEntry->multiModeFlags).c_str());
	SteamMatchmaking()->SetLobbyData(stdComm_steamLobbyID, "tickRateMs", std::to_string(pEntry->tickRateMs).c_str());
	SteamMatchmaking()->SetLobbyData(stdComm_steamLobbyID, "maxRank", std::to_string(pEntry->maxRank).c_str());

	stdComm_bIsServer = 1;
	return 0;
}

int DirectPlay_GetSession_passwordidk(stdCommSession* a)
{
	return 1;
}

int stdComm_EnumSessions(int a, void* b)
{
	if (!SteamAPI_IsSteamRunning())
		return 1;

	lobbyFuncs.ListLobbies();

	return 0;
}

void DirectPlayer_AddPlayer(CSteamID steamID)
{
	const char* nickname = SteamMatchmaking()->GetLobbyMemberData(stdComm_steamLobbyID, steamID, "nickname");
	if(!nickname)
		nickname = SteamFriends()->GetFriendPersonaName(steamID);

	wchar_t wname[128];
	stdString_CharToWchar(wname, nickname, 128);

	DPNAME name;
	name.dwSize = sizeof(DPNAME);
	name.lpszShortName = wname;
	name.lpszLongName = wname;

	DirectPlay_EnumPlayersCallback(steamID.ConvertToUint64(), 0, &name, 0, 0);
}

void DirectPlay_EnumPlayers(int a)
{
	DirectPlay_numPlayers = 0;
	memset(DirectPlay_aPlayers, 0, sizeof(sithDplayPlayer));

	CSteamID lobbyID;
	if (stdComm_dword_8321E0)
		lobbyID = stdComm_steamLobbyID;
	else
		lobbyID = jkGuiMultiplayer_aEntries[a].guidInstance;

	if (!lobbyID.IsValid())
		return;

	int numPlayers = SteamMatchmaking()->GetNumLobbyMembers(lobbyID);
	for (int i = 0; i < numPlayers; ++i)
	{
		CSteamID steamID = SteamMatchmaking()->GetLobbyMemberByIndex(lobbyID, i);
		DirectPlayer_AddPlayer(steamID);
	}
}

int DirectPlay_StartSession(void* a, void* b)
{
	return 1;
}

void DirectPlay_Destroy()
{

}

int DirectPlay_GetSessionDesc(stdCommSession* pEntry)
{
	pEntry->field_E0 = 0;

	if(!lobbyFuncs.GetLobbySessionDesc(stdComm_steamLobbyID, pEntry))
		return 1;

	return 0;
}

int DirectPlay_SortFriends(const void* a, const void* b)
{
	return _wcsicmp(((stdComm_Friend*)a)->name, ((stdComm_Friend*)b)->name);
}

int DirectPlay_EnumFriends()
{
	DirectPlay_ClearFriends();

	DirectPlay_numFriends = SteamFriends()->GetFriendCount(k_EFriendFlagImmediate);
	DirectPlay_apFriends = (stdComm_Friend*)pHS->alloc(sizeof(stdComm_Friend) * DirectPlay_numFriends);
	memset(DirectPlay_apFriends, 0, sizeof(stdComm_Friend) * DirectPlay_numFriends);

	// todo: get this out of here
	rdColor24* pal24 = (rdColor24*)jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_LOAD]->palette;//(rdColor24*)stdDisplay_gammaPalette;

	for (int i = 0; i < DirectPlay_numFriends; ++i)
	{
		CSteamID steamID = SteamFriends()->GetFriendByIndex(i, k_EFriendFlagImmediate);
		DirectPlay_apFriends[i].dpId = steamID.ConvertToUint64();
		stdString_CharToWchar(DirectPlay_apFriends[i].name, SteamFriends()->GetFriendPersonaName(steamID), 32);

		EPersonaState state = SteamFriends()->GetFriendPersonaState(steamID);
		DirectPlay_apFriends[i].state = min((int)state, NET_USER_SNOOZE);

		int avatar = SteamFriends()->GetSmallFriendAvatar(steamID);
		if (avatar > 0 && pal24)
		{
			// If we have to check the size of the image.
			uint32 uAvatarWidth, uAvatarHeight;
			bool success = SteamUtils()->GetImageSize(avatar, &uAvatarWidth, &uAvatarHeight);
			if (success)
			{
				const int uImageSizeInBytes = uAvatarWidth * uAvatarHeight * 4;

				uint8* pixels = (uint8*)std_pHS->alloc(uImageSizeInBytes);
				success = SteamUtils()->GetImageRGBA(avatar, pixels, uImageSizeInBytes);
				if (success)
				{
					//DirectPlay_apFriends[i].thumbnail = stdBitmap_NewEntryFromRGBA(pixels, uAvatarWidth, uAvatarHeight, 0, 0);

					stdVBufferTexFmt vbufTexFmt; // [esp+A0h] [ebp-4Ch] BYREF
					memset(&vbufTexFmt, 0, sizeof(stdVBufferTexFmt));
					vbufTexFmt.format.bpp = 8;
					vbufTexFmt.format.colorMode = STDCOLOR_PAL;
	
					vbufTexFmt.height = uAvatarHeight;
					vbufTexFmt.width = uAvatarWidth;

					DirectPlay_apFriends[i].thumbnail = stdDisplay_VBufferNew(&vbufTexFmt, 0, 0, 0);
					if (DirectPlay_apFriends[i].thumbnail)
					{
						DirectPlay_apFriends[i].thumbnail->palette = pal24;
						stdDisplay_VBufferLock(DirectPlay_apFriends[i].thumbnail);
						uint8_t* target = (uint8_t*)DirectPlay_apFriends[i].thumbnail->surface_lock_alloc;
						for (int y = 0; y < uAvatarHeight; ++y)
						{
							for (int x = 0; x < uAvatarWidth; ++x)
							{
								uint32_t pixelIdx = (y * uAvatarWidth + x);

								rdColor32 rgb = ((rdColor32*)pixels)[pixelIdx];
												
								uint8_t idx = 0;
								float maxDist = FLT_MAX;
								for (int k = 0; k < 256; ++k)
								{
									float dr = ((float)rgb.r - (float)pal24[k].r) / 255.0f;
									float dg = ((float)rgb.g - (float)pal24[k].g) / 255.0f;
									float db = ((float)rgb.b - (float)pal24[k].b) / 255.0f;

									float dist = (dr * dr + dg * dg + db * db);
									if(dist < maxDist)
									{
										idx = k;
										maxDist = dist;
									}
								}
						
								target[pixelIdx] = idx;
							}
						}
						//memcpy(DirectPlay_apFriends[i].thumbnail->surface_lock_alloc, pixels, uImageSizeInBytes);
						stdDisplay_VBufferUnlock(DirectPlay_apFriends[i].thumbnail);
					}
				}
				std_pHS->free(pixels);
			}
		}
	}

	qsort(DirectPlay_apFriends, DirectPlay_numFriends, sizeof(stdComm_Friend), DirectPlay_SortFriends);

	return 1;
}

void DirectPlay_Invite(DPID id)
{
	CSteamID steamID(id);
	if(steamID.IsValid())
		SteamMatchmaking()->InviteUserToLobby(stdComm_steamLobbyID, steamID);
}

}

#else

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h> // Ug, for NukeProcess -- see below
    #include <ws2tcpip.h>
    typedef int socklen_t;
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <sys/ioctl.h>
    typedef int SOCKET;
    constexpr SOCKET INVALID_SOCKET = -1;
    #include <signal.h>
    #include <dlfcn.h>
#endif

#define sithDplaySteam_verbosePrintf(fmt, ...) if (Main_bVerboseNetworking) \
    { \
        stdPlatform_Printf(fmt, ##__VA_ARGS__);  \
    } \
    ;

extern "C"
{
void Hack_ResetClients();

#pragma pack(push, 4)
typedef struct GNSInfoPacket
{
    int id;
    stdCommSession entry;
} GNSInfoPacket;
#pragma pack(pop)

static stdCommSession sithDplayGNS_storedEntryEnum;
static stdCommSession sithDplayGNS_storedEntry;

static int sithDplayGNS_numEnumd = 0;
extern int Main_bVerboseNetworking;

const uint16 DEFAULT_SERVER_PORT = 27020;

CSteamID stdComm_steamLobbyID;

}
/////////////////////////////////////////////////////////////////////////////
//
// Common stuff
//
/////////////////////////////////////////////////////////////////////////////

bool g_bQuit = false;

SteamNetworkingMicroseconds g_logTimeZero;

#ifdef WIN32
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h>   // for nanosleep
#endif

void sleep_ms(int milliseconds){ // cross-platform sleep function
    SDL_Delay(milliseconds);
}

// We do this because I won't want to figure out how to cleanly shut
// down the thread that is reading from stdin.
static void NukeProcess( int rc )
{
    #ifdef _WIN32
        ExitProcess( rc );
    #else
        (void)rc; // Unused formal parameter
        kill( getpid(), SIGKILL );
    #endif
}

static void DebugOutput( ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg )
{
    //SteamNetworkingMicroseconds time = g_SteamNetworkingUtils()->GetLocalTimestamp() - g_logTimeZero;
    //sithDplaySteam_verbosePrintf( "%10.6f %s\n", time*1e-6, pszMsg );
    sithDplaySteam_verbosePrintf( "%s\n", pszMsg );
    fflush(stdout);
    if ( eType == k_ESteamNetworkingSocketsDebugOutputType_Bug )
    {
        fflush(stdout);
        fflush(stderr);
        //NukeProcess(1);
    }
}

static void FatalError( const char *fmt, ... )
{
    char text[ 2048 ];
    va_list ap;
    va_start( ap, fmt );
    vsnprintf( text, sizeof(text), fmt, ap );
    va_end(ap);
    char *nl = strchr( text, '\0' ) - 1;
    if ( nl >= text && *nl == '\n' )
        *nl = '\0';
    DebugOutput( k_ESteamNetworkingSocketsDebugOutputType_Bug, text );
}

static void Printf( const char *fmt, ... )
{
    char text[ 2048 ];
    va_list ap;
    va_start( ap, fmt );
    vsnprintf( text, sizeof(text), fmt, ap );
    va_end(ap);
    char *nl = strchr( text, '\0' ) - 1;
    if ( nl >= text && *nl == '\n' )
        *nl = '\0';
    DebugOutput( k_ESteamNetworkingSocketsDebugOutputType_Msg, text );
}

/////////////////////////////////////////////////////////////////////////////
//
// GNSServer
//
/////////////////////////////////////////////////////////////////////////////

class GNSServer
{
public:
    void Init( /*uint16 nPort*/)
    {
        // Select instance to use.  For now we'll always use the default.
        // But we could use SteamGameServerNetworkingSockets() on Steam.
        m_pInterface = SteamNetworkingSockets();

        // Start listening
        SteamNetworkingIPAddr serverLocalAddr;
        serverLocalAddr.Clear();
		serverLocalAddr.ParseString("127.0.0.1");
		serverLocalAddr.m_port = DEFAULT_SERVER_PORT;

        SteamNetworkingConfigValue_t opts[3];
		opts[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)SteamNetConnectionStatusChangedCallback);
        //opt.SetPtr( k_ESteamNetworkingConfig_Callback_CreateConnectionSignaling, (void*)SteamNetCreateConnectionSignalingCallback);
        //m_hListenSock = m_pInterface->CreateListenSocketIP( serverLocalAddr, 1, &opt );
		opts[1].SetInt32(k_ESteamNetworkingConfig_TimeoutInitial, 25000);
		opts[2].SetInt32(k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 2);

		m_hListenSockLocal = m_pInterface->CreateListenSocketIP(serverLocalAddr, 3, opts);
		if (m_hListenSockLocal == k_HSteamListenSocket_Invalid)
			Printf("[1] Failed to listen locally on port %d", 0);//nPort );

		m_hListenSock = m_pInterface->CreateListenSocketP2P(0, 3, opts);
        if ( m_hListenSock == k_HSteamListenSocket_Invalid )
            Printf( "[1] Failed to listen on port %d", 0);//nPort );
        m_hPollGroup = m_pInterface->CreatePollGroup();
        if ( m_hPollGroup == k_HSteamNetPollGroup_Invalid )
            Printf( "[2] Failed to listen on port %d", 0);//nPort );
        Printf( "Server listening on port %d\n", 0);//nPort );

        //m_pBcastInterface = SteamNetworkingMessages();

        m_identity.Clear();
        //m_identity.SetGenericString("OpenJKDF2");
		m_identity.SetSteamID(SteamUser()->GetSteamID());

		SteamAPICall_t hSteamAPICall = SteamMatchmaking()->CreateLobby(k_ELobbyTypePublic, sithDplayGNS_storedEntry.maxPlayers);
		steamCallResultLobbyCreated.Set(hSteamAPICall, this, &GNSServer::OnLobbyCreated);
		SteamAPI_RunCallbacks();

        id = 1;
        availableIds = 0x1;
    }

    void Shutdown()
    {
        // Close all the connections
        Printf( "Closing connections...\n" );
        for ( auto it: m_mapClients )
        {
            // TODO: Send a proper shutdown message

            // Close the connection.  We use "linger mode" to ask SteamNetworkingSockets
            // to flush this out and close gracefully.
            m_pInterface->CloseConnection( it.first, 0, "Server Shutdown", true );
        }
        m_mapClients.clear();

        m_pInterface->CloseListenSocket( m_hListenSock );
        m_hListenSock = k_HSteamListenSocket_Invalid;

		m_pInterface->CloseListenSocket(m_hListenSockLocal);
		m_hListenSockLocal = k_HSteamListenSocket_Invalid;

        m_pInterface->DestroyPollGroup( m_hPollGroup );
        m_hPollGroup = k_HSteamNetPollGroup_Invalid;

		if(lobbyID.IsValid())
			SteamMatchmaking()->LeaveLobby(lobbyID);

        availableIds = 0x1;
    }

    void RunStep()
    {
        //printf("Server runstep\n");
        //PollIncomingMessages();
        PollConnectionStateChanges();
        //TickBroadcastOut();
    }

    void Run()
    {
        while ( !g_bQuit )
        {
            RunStep();
            sleep_ms(10);
        }

        Shutdown();
    }

    int Receive(int *pIdOut, void *pMsg, int *pLenInOut)
    {
        int maxLen = *pLenInOut;
        *pIdOut = 0;
        *pLenInOut = 0;

        if (m_DisconnectedPeers.size())
        {
            int dis_id = m_DisconnectedPeers.front();
            m_DisconnectedPeers.pop();

            *pIdOut = dis_id;

            return 2;
        }

        ISteamNetworkingMessage *pIncomingMsg = nullptr;
        int numMsgs = m_pInterface->ReceiveMessagesOnPollGroup( m_hPollGroup, &pIncomingMsg, 1 );
        if ( numMsgs == 0 )
            return -1;
        if ( numMsgs < 0 ) {
            stdPlatform_Printf( "Error checking for messages (%d)\n", numMsgs);
            return -1;
        }
        assert( numMsgs == 1 && pIncomingMsg );
        auto itClient = m_mapClients.find( pIncomingMsg->m_conn );
        assert( itClient != m_mapClients.end() );

        if (pIncomingMsg->m_cbSize < 8) {
            stdPlatform_Printf("Bad packet size %x\n", pIncomingMsg->m_cbSize);
            pIncomingMsg->Release();
            return -1;
        }

        uint8_t* dataBuf = (uint8_t*)pIncomingMsg->m_pData;
        int idFrom = *(uint32_t*)&dataBuf[0];//itClient->second.m_id;
        int idTo = *(uint32_t*)&dataBuf[4];
        *pIdOut = idFrom;

        // If we get a packet intended for another client, forward it to them.
        if (idTo && idTo != id)
        {
            Send(idFrom, idTo, &dataBuf[8], pIncomingMsg->m_cbSize-8);
            pIncomingMsg->Release();

            *pLenInOut = maxLen;
            return Receive(pIdOut, pMsg, pLenInOut);
        }

        int outsize = maxLen;
        if (outsize > pIncomingMsg->m_cbSize-8)
            outsize = pIncomingMsg->m_cbSize-8;

        memcpy(pMsg, &dataBuf[8], outsize);
        *pLenInOut = outsize;

        sithDplaySteam_verbosePrintf("Recv %x bytes from %x %x (%s, %x)\n", outsize, idFrom, idTo, sithDSS_IdToStr(*(uint32_t*)pMsg & 0xFF), *(uint32_t*)pMsg);

        // We don't need this anymore.
        pIncomingMsg->Release();

        return 0;
    }

    int Send(uint32_t idFrom, uint32_t idTo, void *lpData, uint32_t dwDataSize)
    {
        if (dwDataSize > 4096-8) dwDataSize = 4096-8;
        *(uint32_t*)&sendBuffer[0] = idFrom;
        *(uint32_t*)&sendBuffer[4] = idTo;

        memcpy(&sendBuffer[8], lpData, dwDataSize);

        HSteamNetConnection except = k_HSteamNetConnection_Invalid;
        for ( auto &c: m_mapClients )
        {
            if ( c.first != except && c.second.m_id == idTo ) {
                sithDplaySteam_verbosePrintf("Sent %x bytes to %x (%s, %x)\n", dwDataSize+8, idTo, sithDSS_IdToStr(*(uint32_t*)lpData & 0xFF), *(uint32_t*)lpData);
                SendBytesToClient( c.first, sendBuffer, dwDataSize+8 );
            }
        }

        return 1;
    }

    uint32_t id;
private:
	CCallResult<GNSServer, LobbyCreated_t> steamCallResultLobbyCreated;

    HSteamListenSocket m_hListenSock;
	HSteamListenSocket m_hListenSockLocal;
    HSteamNetPollGroup m_hPollGroup;
    ISteamNetworkingSockets *m_pInterface;
    //ISteamNetworkingMessages *m_pBcastInterface;
    uint64_t availableIds = 0x1;
    uint8_t sendBuffer[4096];
    uint8_t sendBuffer2[4096];
    SteamNetworkingIdentity m_identity;

    struct Client_t
    {
        uint32_t m_id;
        std::string m_sNick;
    };

    std::map< HSteamNetConnection, Client_t > m_mapClients;
    std::queue<int> m_DisconnectedPeers;

    void SendBytesToClient( HSteamNetConnection conn, void *pData, uint32_t len)
    {
        m_pInterface->SendMessageToConnection( conn, pData, len, k_nSteamNetworkingSend_Reliable, nullptr );
    }

    void SendBytesToAllClients( void *pData, uint32_t len, HSteamNetConnection except = k_HSteamNetConnection_Invalid )
    {
        for ( auto &c: m_mapClients )
        {
            if ( c.first != except )
                SendBytesToClient( c.first, pData, len );
        }
    }

    void SetClientNick( HSteamNetConnection hConn, const char *nick )
    {

        // Remember their nick
        m_mapClients[hConn].m_sNick = nick;

        // Set the connection name, too, which is useful for debugging
        m_pInterface->SetConnectionName( hConn, nick );
    }

    void SetClientId( HSteamNetConnection hConn, int id )
    {
        m_mapClients[hConn].m_id = id;
    }

	void OnLobbyCreated(LobbyCreated_t* callback, bool IOFailure)
	{
		if (callback->m_eResult == k_EResultOK)
		{
			stdComm_steamLobbyID = callback->m_ulSteamIDLobby;

			// make sure we're the owner
			SteamMatchmaking()->SetLobbyOwner(callback->m_ulSteamIDLobby, SteamUser()->GetSteamID());

			char serverName[32];
			stdString_WcharToChar(serverName, sithDplayGNS_storedEntry.serverName, 32);

			SteamMatchmaking()->SetLobbyData(callback->m_ulSteamIDLobby, "serverName", serverName);
			SteamMatchmaking()->SetLobbyData(callback->m_ulSteamIDLobby, "episodeGobName", sithDplayGNS_storedEntry.episodeGobName);
			SteamMatchmaking()->SetLobbyData(callback->m_ulSteamIDLobby, "mapJklFname", sithDplayGNS_storedEntry.mapJklFname);
			SteamMatchmaking()->SetLobbyData(callback->m_ulSteamIDLobby, "maxRank", std::to_string(sithDplayGNS_storedEntry.maxRank).c_str());

			sithDplayGNS_storedEntry.maxPlayers = SteamMatchmaking()->GetLobbyMemberLimit(stdComm_steamLobbyID);
			sithDplayGNS_storedEntry.numPlayers = SteamMatchmaking()->GetNumLobbyMembers(stdComm_steamLobbyID);
		}
	}

    void OnSteamNetConnectionStatusChanged( SteamNetConnectionStatusChangedCallback_t *pInfo )
    {
        char temp[1024];

        // What's the state of the connection?
        switch ( pInfo->m_info.m_eState )
        {
            case k_ESteamNetworkingConnectionState_None:
                // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
                break;

            case k_ESteamNetworkingConnectionState_ClosedByPeer:
            case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            {
                // Ignore if they were not previously connected.  (If they disconnected
                // before we accepted the connection.)
                if ( pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connected )
                {

                    // Locate the client.  Note that it should have been found, because this
                    // is the only codepath where we remove clients (except on shutdown),
                    // and connection change callbacks are dispatched in queue order.
                    auto itClient = m_mapClients.find( pInfo->m_hConn );
                    assert( itClient != m_mapClients.end() );

                    // Select appropriate log messages
                    const char *pszDebugLogAction;
                    if ( pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally )
                    {
                        pszDebugLogAction = "problem detected locally";
                        snprintf( temp, sizeof(temp), "Problem detected with client %x (%s)", itClient->second.m_id, pInfo->m_info.m_szEndDebug );
                    }
                    else
                    {
                        // Note that here we could check the reason code to see if
                        // it was a "usual" connection or an "unusual" one.
                        pszDebugLogAction = "closed by peer";
                        snprintf( temp, sizeof(temp), "Client id %x has left.", itClient->second.m_id );
                    }

                    // Spew something to our own log.  Note that because we put their nick
                    // as the connection description, it will show up, along with their
                    // transport-specific data (e.g. their IP address)
                    Printf( "Connection %s %s, reason %d: %s\n",
                        pInfo->m_info.m_szConnectionDescription,
                        pszDebugLogAction,
                        pInfo->m_info.m_eEndReason,
                        pInfo->m_info.m_szEndDebug
                    );

                    // Only send disconnect messages to fully connected clients.
                    for (int i = 0; i < jkPlayer_maxPlayers; i++)
                    {
                        if (!i && jkGuiNetHost_bIsDedicated) continue;

                        if ( (jkPlayer_playerInfos[i].flags & 2) != 0 && jkPlayer_playerInfos[i].net_id == itClient->second.m_id) {
                            m_DisconnectedPeers.push(itClient->second.m_id);
                            break;
                        }
                    }

                    availableIds &= ~(1ULL << (itClient->second.m_id-1));
                    m_mapClients.erase( itClient );
                }
                else
                {
                    assert( pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting );
                }

                // Clean up the connection.  This is important!
                // The connection is "closed" in the network sense, but
                // it has not been destroyed.  We must close it on our end, too
                // to finish up.  The reason information do not matter in this case,
                // and we cannot linger because it's already closed on the other end,
                // so we just pass 0's.
                m_pInterface->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
                break;
            }

            case k_ESteamNetworkingConnectionState_Connecting:
            {
                // This must be a new connection
                assert( m_mapClients.find( pInfo->m_hConn ) == m_mapClients.end() );

                Printf( "Connection request from %s", pInfo->m_info.m_szConnectionDescription );

                // Don't accept if we can't allocate an ID
                if (ConnectedPlayers() >= 64) {
                    stdPlatform_Printf("Rejecting request too many players connected (%u)\n", ConnectedPlayers());
                    break;
                }

                // A client is attempting to connect
                // Try to accept the connection.
                if ( m_pInterface->AcceptConnection( pInfo->m_hConn ) != k_EResultOK )
                {
                    // This could fail.  If the remote host tried to connect, but then
                    // disconnected, the connection may already be half closed.  Just
                    // destroy whatever we have on our side.
                    m_pInterface->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
                    Printf( "Can't accept connection.  (It was already closed?)" );
                    break;
                }

                // Assign the poll group
                if ( !m_pInterface->SetConnectionPollGroup( pInfo->m_hConn, m_hPollGroup ) )
                {
                    m_pInterface->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
                    Printf( "Failed to set poll group?" );
                    break;
                }

                int nextId = 0;
                for (int i = 0; i < 64; i++)
                {
                    if (!(availableIds & (1ULL << i))) {
                        availableIds |= (1ULL << i);
                        nextId = i+1;
                        break;
                    }
                }

                sithDplaySteam_verbosePrintf("Assigning ID: %x\n", nextId);

                sithDplayGNS_storedEntry.multiModeFlags = sithMulti_multiModeFlags;

                GNSInfoPacket infoPkt = {0};
                infoPkt.id = nextId;
                infoPkt.entry = sithDplayGNS_storedEntry;
                infoPkt.entry.numPlayers = RealConnectedPlayers();
                infoPkt.entry.maxPlayers = sithDplayGNS_storedEntry.maxPlayers;

                jkPlayer_maxPlayers = sithDplayGNS_storedEntry.maxPlayers; // Hack?

                memcpy(sendBuffer2, &infoPkt, sizeof(infoPkt));
                SendBytesToClient( pInfo->m_hConn, sendBuffer2, sizeof(infoPkt)); 

                // Add them to the client list, using std::map wacky syntax
                m_mapClients[ pInfo->m_hConn ];
                SetClientNick( pInfo->m_hConn, "asdf" );
                SetClientId( pInfo->m_hConn, nextId);

                nextId++;
                break;
            }

            case k_ESteamNetworkingConnectionState_Connected:
                // We will get a callback immediately after accepting the connection.
                // Since we are the server, we can ignore this, it's not news to us.
                break;

            default:
                // Silences -Wswitch
                break;
        }
    }

    static GNSServer *s_pCallbackInstance;
    static void SteamNetConnectionStatusChangedCallback( SteamNetConnectionStatusChangedCallback_t *pInfo )
    {
        s_pCallbackInstance->OnSteamNetConnectionStatusChanged( pInfo );
    }

    static ISteamNetworkingConnectionSignaling* SteamNetCreateConnectionSignalingCallback( ISteamNetworkingSockets *pLocalInterface, const SteamNetworkingIdentity &identityPeer, int nLocalVirtualPort, int nRemoteVirtualPort )
    {
        //s_pCallbackInstance->OnSteamNetConnectionStatusChanged( pInfo );
        sithDplaySteam_verbosePrintf("incoming!\n");
        return nullptr;
    }

    void PollConnectionStateChanges()
    {
        s_pCallbackInstance = this;
        m_pInterface->RunCallbacks();
		SteamAPI_RunCallbacks();
	}

    int ConnectedPlayers()
    {
        int ret = 0;
        //RealConnectedPlayers();
        for (int i = 0; i < 64; i++)
        {
            if (availableIds & (1ULL << i)) {
                ret++;
            }
        }
        return ret;
    }

    int RealConnectedPlayers()
    {
        int amt = 0;
        //availableIds = 3;
        for (int i = 0; i < jkPlayer_maxPlayers; i++)
        {
            if (!i && jkGuiNetHost_bIsDedicated) continue;


            if ( (jkPlayer_playerInfos[i].flags & 2) != 0 && !jkPlayer_playerInfos[i].net_id ){
                
            }
            else {
                //availableIds |= (1ULL << (jkPlayer_playerInfos[i].net_id-1));
                amt++;
            }
        }
        return amt;
    }
};

GNSServer *GNSServer::s_pCallbackInstance = nullptr;

/////////////////////////////////////////////////////////////////////////////
//
// GNSClient
//
/////////////////////////////////////////////////////////////////////////////

class GNSClient
{
public:
    void Init()
    {
        id = 0xFFFFFFFF;
        m_closed = 0;

        // Select instance to use.  For now we'll always use the default.
        m_pInterface = SteamNetworkingSockets();

		CSteamID hostID = SteamMatchmaking()->GetLobbyOwner(stdComm_steamLobbyID);

        // Start connecting
        //char szAddr[ SteamNetworkingIPAddr::k_cchMaxString ];
		//SteamNetworkingUtils()->SteamNetworkingIPAddr_ToString(serverAddr, szAddr, sizeof(szAddr), true );
       // Printf( "Connecting to server at %s", szAddr );
        SteamNetworkingConfigValue_t opt[3];
        opt[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)SteamNetConnectionStatusChangedCallback);
		opt[1].SetInt32(k_ESteamNetworkingConfig_TimeoutInitial, 25000);
		opt[2].SetInt32(k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 2);
	  //opt.SetPtr( k_ESteamNetworkingConfig_Callback_CreateConnectionSignaling, (void*)SteamNetCreateConnectionSignalingCallback);
		if(hostID == SteamUser()->GetSteamID())
		{
			SteamNetworkingIPAddr serverLocalAddr;
			serverLocalAddr.Clear();
			serverLocalAddr.ParseString("127.0.0.1");
			serverLocalAddr.m_port = DEFAULT_SERVER_PORT;
			m_hConnection = m_pInterface->ConnectByIPAddress(serverLocalAddr, 3, opt);
		}
		else
		{		
			SteamNetworkingIdentity hostIdentity;
			hostIdentity.SetSteamID(hostID);
			m_hConnection = m_pInterface->ConnectP2P(hostIdentity, 0, 3, opt);
		}

        if ( m_hConnection == k_HSteamNetConnection_Invalid ) {
            Printf( "Failed to create connection" );
            m_closed = 1;
        }

        //m_pBcastInterface = SteamNetworkingMessages();

        m_identity.Clear();
        m_identity.SetGenericString("OpenJKDF2");

        m_hostDisconnected = 0;

		//SteamAPI_RunCallbacks();
    }

    void Shutdown()
    {
		if(SteamMatchmaking()->GetLobbyOwner(stdComm_steamLobbyID) != SteamUser()->GetSteamID()) // we might be connected locally, don't leave in that case
			SteamMatchmaking()->LeaveLobby(stdComm_steamLobbyID);

        m_pInterface->CloseConnection( m_hConnection, 0, nullptr, false );
        m_hConnection = k_HSteamNetConnection_Invalid;
        id = 0xFFFFFFFF;
        m_closed = 1;

        m_hostDisconnected = 0;
    }

    void RunStep()
    {
        //printf("Client runstep\n");
        //PollIncomingMessages();
        PollConnectionStateChanges();
        //TickBroadcastIn();
    }

    void Run()
    {
        while ( !g_bQuit )
        {
            RunStep();
            sleep_ms(10);
        }
    }

    int Receive(int *pIdOut, void *pMsg, int *pLenInOut)
    {
        int maxLen = *pLenInOut;
        *pIdOut = 0;
        *pLenInOut = 0;

        if ( m_hostDisconnected ) {
            stdPlatform_Printf( "Host is disconnected, forcing exit...\n");
            Shutdown();
            m_closed = 1;
            *pIdOut = 1;
            return 2;
        }

        ISteamNetworkingMessage *pIncomingMsg = nullptr;
        int numMsgs = m_pInterface->ReceiveMessagesOnConnection( m_hConnection, &pIncomingMsg, 1 );
        if ( numMsgs == 0 )
            return -1;
        if ( numMsgs < 0 ) {
            stdPlatform_Printf( "Error checking for messages (%d)\n", numMsgs);
            Shutdown();
            m_closed = 1;
            *pIdOut = 1;
            m_hackFallback = !m_hackFallback;
            return m_hackFallback ? 2 : -1;
        }

        if (pIncomingMsg->m_cbSize < 8) {
            stdPlatform_Printf("Bad packet size %x\n", pIncomingMsg->m_cbSize);
            Shutdown();
            m_closed = 1;
            return -1;
        }

        uint8_t* dataBuf = (uint8_t*)pIncomingMsg->m_pData;
        int idFrom = *(uint32_t*)&dataBuf[0];//itClient->second.m_id;
        int idTo = *(uint32_t*)&dataBuf[4];
        *pIdOut = idFrom;

        // Not intended for us
        if (idTo && idTo != id) {
            return -1;
        }
        
        int outsize = maxLen;
        if (outsize > pIncomingMsg->m_cbSize-8)
            outsize = pIncomingMsg->m_cbSize-8;

        memcpy(pMsg, &dataBuf[8], outsize);
        *pLenInOut = outsize;

        sithDplaySteam_verbosePrintf("Recv %x bytes from %x %x (%s, %x)\n", pIncomingMsg->m_cbSize, idFrom, idTo, sithDSS_IdToStr(*(uint32_t*)pMsg & 0xFF), *(uint32_t*)pMsg);

        // We don't need this anymore.
        pIncomingMsg->Release();

        return 0;
    }

    int Send(uint32_t idFrom, uint32_t idTo, void *lpData, uint32_t dwDataSize)
    {
        if (dwDataSize > 4096-8) dwDataSize = 4096-8;
        *(uint32_t*)&sendBuffer[0] = idFrom;
        *(uint32_t*)&sendBuffer[4] = idTo;

        memcpy(&sendBuffer[8], lpData, dwDataSize);

        sithDplaySteam_verbosePrintf("Sent %x bytes to %x (%s, %x)\n", dwDataSize+8, idTo, sithDSS_IdToStr(*(uint32_t*)lpData & 0xFF), *(uint32_t*)lpData);

        EResult ret = m_pInterface->SendMessageToConnection( m_hConnection, sendBuffer, dwDataSize+8, k_nSteamNetworkingSend_Reliable, nullptr );
        if (ret < 0) {
            stdPlatform_Printf( "Error sending message (%d)\n", ret);
        }
        if (ret == k_EResultNoConnection || ret == k_EResultInvalidParam) {
            return 0;
        }

        return 1;
    }

    void GetServerInfo( const SteamNetworkingIPAddr &serverAddr )
    {
       // int attempts = 1;
       // int id_real = 0xFFFFFFFF;
       // id = 0xFFFFFFFF;
       // m_closed = 0;
       // 
       // while (id == 0xFFFFFFFF && !m_closed && attempts) {
       //     Init(serverAddr);
       //     for (int i = 0; i < 100; i++)
       //     {
       //         RunStep();
       //         sleep_ms(10);
       //         if (id != 0xFFFFFFFF) break;
       //     }
       //     id_real = id;
       //     Shutdown();
       //     attempts--;
       // }
       // 
       // if (id_real == 0xFFFFFFFF) {
       //     sithDplayGNS_numEnumd = 0;
       // }
	   //
       // // Hack: Force the UI to update
       // jkGuiMultiplayer_dword_5564E8 -= 10000;
    }

    uint32_t id = 0xFFFFFFFF;
private:

    HSteamNetConnection m_hConnection;
    ISteamNetworkingSockets *m_pInterface;
    //ISteamNetworkingMessages *m_pBcastInterface;
    SteamNetworkingIdentity m_identity;
    uint8_t sendBuffer[4096];
    int m_closed = 0;
    int m_hostDisconnected = 0;
    int m_hackFallback = 0;

    void PollIncomingMessages()
    {
        ISteamNetworkingMessage *pIncomingMsg = nullptr;
        int numMsgs = m_pInterface->ReceiveMessagesOnConnection( m_hConnection, &pIncomingMsg, 1 );
        if ( numMsgs == 0 )
            return;
        if ( numMsgs < 0 ) {
            stdPlatform_Printf( "Error checking for messages (%d)\n", numMsgs);
            return;
        }

        // Just echo anything we get from the server
        sithDplaySteam_verbosePrintf("Received %x bytes (%x)\n", pIncomingMsg->m_cbSize, sizeof(GNSInfoPacket));

        if (id == 0xFFFFFFFF && pIncomingMsg->m_cbSize == sizeof(GNSInfoPacket)) {
            GNSInfoPacket* pPkt = (GNSInfoPacket*)pIncomingMsg->m_pData;
            id = pPkt->id;
            sithDplaySteam_verbosePrintf("We are ID %x\n", id);

            sithDplayGNS_storedEntryEnum = pPkt->entry;
            sithDplayGNS_storedEntryEnum.field_E0 = 10;

            // Hack?
            sithMulti_multiModeFlags = sithDplayGNS_storedEntryEnum.multiModeFlags;
            sithNet_MultiModeFlags = sithDplayGNS_storedEntryEnum.multiModeFlags;
            sithDplayGNS_numEnumd = 1;
        }

        // We don't need this anymore.
        pIncomingMsg->Release();
    }

#if 0
    int TickBroadcastIn()
    {
        SteamNetworkingMessage_t *pMsg = nullptr;
        int numMsgs = m_pBcastInterface->ReceiveMessagesOnChannel(1337, &pMsg, 1);
        if ( numMsgs == 0 )
            return -1;
        if ( numMsgs < 0 ) {
            stdPlatform_Printf( "Error checking for messages (%d)\n", numMsgs);
            Shutdown();
            m_closed = 1;
            return 2;
        }

        if (pMsg->m_cbSize < 8) {
            stdPlatform_Printf("Bad packet size %x\n", pMsg->m_cbSize);
            Shutdown();
            m_closed = 1;
            return -1;
        }

        stdPlatform_Printf("Got broadcast %x\n", pMsg->m_cbSize);
        return 0;
    }
#endif

    void OnSteamNetConnectionStatusChanged( SteamNetConnectionStatusChangedCallback_t *pInfo )
    {
        if (m_hConnection == k_HSteamNetConnection_Invalid )
            return;

        // What's the state of the connection?
        switch ( pInfo->m_info.m_eState )
        {
            case k_ESteamNetworkingConnectionState_None:
                // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
                break;

            case k_ESteamNetworkingConnectionState_ClosedByPeer:
            case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            {
                g_bQuit = true;

                // Print an appropriate message
                if ( pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting )
                {
                    // Note: we could distinguish between a timeout, a rejected connection,
                    // or some other transport problem.
                    Printf( "Couldn't connect to host. (%s)", pInfo->m_info.m_szEndDebug );
                }
                else if ( pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally )
                {
                    Printf( "Lost contact with the host. (%s)", pInfo->m_info.m_szEndDebug );
                }
                else
                {
                    // NOTE: We could check the reason code for a normal disconnection
                    Printf( "Host has disconnected. (%s)", pInfo->m_info.m_szEndDebug );
                    m_hostDisconnected = 1;
                }

                // Clean up the connection.  This is important!
                // The connection is "closed" in the network sense, but
                // it has not been destroyed.  We must close it on our end, too
                // to finish up.  The reason information do not matter in this case,
                // and we cannot linger because it's already closed on the other end,
                // so we just pass 0's.
                m_pInterface->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
                m_hConnection = k_HSteamNetConnection_Invalid;
                break;
            }

            case k_ESteamNetworkingConnectionState_Connecting:
                // We will get this callback when we start connecting.
                // We can ignore this.
                break;

            case k_ESteamNetworkingConnectionState_Connected:
                Printf( "Connected to server OK" );
                while (id == 0xFFFFFFFF) {
                    PollIncomingMessages();
                }
                Hack_ResetClients();
                break;

            default:
                // Silences -Wswitch
                break;
        }
    }

    static GNSClient *s_pCallbackInstance;
    static void SteamNetConnectionStatusChangedCallback( SteamNetConnectionStatusChangedCallback_t *pInfo )
    {
        s_pCallbackInstance->OnSteamNetConnectionStatusChanged( pInfo );
    }

    static ISteamNetworkingConnectionSignaling* SteamNetCreateConnectionSignalingCallback( ISteamNetworkingSockets *pLocalInterface, const SteamNetworkingIdentity &identityPeer, int nLocalVirtualPort, int nRemoteVirtualPort )
    {
        //s_pCallbackInstance->OnSteamNetConnectionStatusChanged( pInfo );
        stdPlatform_Printf("incoming!\n");
        return nullptr;
    }

    void PollConnectionStateChanges()
    {
        s_pCallbackInstance = this;
        m_pInterface->RunCallbacks();
		SteamAPI_RunCallbacks();
	}
};

GNSClient *GNSClient::s_pCallbackInstance = nullptr;





extern "C"
{

extern int jkGuiNetHost_portNum;

GNSClient client;
GNSServer server;

void  SteamWarningMessageHook(int severity, const char* msg)
{
	Printf(msg);
}

void Hack_ResetClients()
{
    DirectPlay_numPlayers = 32;
    for (int i = 0; i < 32; i++)
    {
        DirectPlay_aPlayers[i].dpId = i+1;
        jk_snwprintf(DirectPlay_aPlayers[i].waName, 32, L"asdf");
    }

    int id_self = 1;
    int id_other = 2;
    if (!stdComm_bIsServer)
    {
        id_self = 2;
        id_other = 1;
    }
    //jkPlayer_playerInfos[0].net_id = id_self;
    //jkPlayer_playerInfos[1].net_id = id_other;
    //jk_snwprintf(jkPlayer_playerInfos[0].player_name, 32, "asdf1");
    //jk_snwprintf(jkPlayer_playerInfos[1].player_name, 32, "asdf2");

    //jkPlayer_maxPlayers = 2;

    if (stdComm_bIsServer)
        stdComm_dplayIdSelf = server.id;
    else
        stdComm_dplayIdSelf = client.id;
}

void stdComm_Steam_Startup()
{
	if (!SteamAPI_Init())
    {
        jkGuiMultiplayer_numConnections = 1;
        jk_snwprintf(jkGuiMultiplayer_aConnections[0].name, 0x80, L"Screaming Into The Void (Steam Init Failed)");
        stdComm_dword_8321E0 = 0;

        memset(jkGuiMultiplayer_aEntries, 0, sizeof(stdCommSession) * 32);
        dplay_dword_55D618 = 0;
        return;
    }

	SteamUtils()->SetWarningMessageHook(&SteamWarningMessageHook);

	SteamNetworkingUtils()->InitRelayNetworkAccess();

    jkGuiMultiplayer_numConnections = 1;
    jk_snwprintf(jkGuiMultiplayer_aConnections[0].name, 0x80, L"Steam");
    stdComm_dword_8321E0 = 0;

    memset(jkGuiMultiplayer_aEntries, 0, sizeof(stdCommSession) * 32);
    dplay_dword_55D618 = 0;
    /*jk_snwprintf(jkGuiMultiplayer_aEntries[0].serverName, 0x20, L"OpenJKDF2 Loopback");
    stdString_snprintf(jkGuiMultiplayer_aEntries[0].episodeGobName, 0x20, "JK1MP");
    stdString_snprintf(jkGuiMultiplayer_aEntries[0].mapJklFname, 0x20, "m2.jkl");
    jkGuiMultiplayer_aEntries[0].field_E0 = 10;*/

    Hack_ResetClients();
}

void stdComm_Steam_Shutdown()
{
	SteamAPI_Shutdown();
}

int DirectPlay_Receive(int *pIdOut, int *pMsgIdOut, int *pLenOut)
{
    if (!SteamAPI_IsSteamRunning())
        return -1;

    Hack_ResetClients();

    if (stdComm_bIsServer)
    {
        server.RunStep();
        return server.Receive(pIdOut, (void*)pMsgIdOut, pLenOut);
    }
    else 
    {
        client.RunStep();
        return client.Receive(pIdOut, (void*)pMsgIdOut, pLenOut);
    }

    return -1;
}

BOOL DirectPlay_Send(DPID idFrom, DPID idTo, void *lpData, DWORD dwDataSize)
{
    if (!SteamAPI_IsSteamRunning())
        return 0;

    Hack_ResetClients();

    if (stdComm_bIsServer)
    {
        server.RunStep();
        return server.Send(idFrom, idTo, lpData, dwDataSize);
    }
    else 
    {
        client.RunStep();
        return client.Send(idFrom, idTo, lpData, dwDataSize);
    }

    return 1;
}

int stdComm_OpenConnection(void* a)
{
    stdComm_dword_8321DC = 1;
    return 0;
}

void stdComm_CloseConnection()
{
    if ( stdComm_dword_8321DC )
    {
        if ( stdComm_dword_8321E0 )
        {
            //DirectPlay_DestroyPlayer(stdComm_dplayIdSelf);
            DirectPlay_Close();
            stdComm_dword_8321E0 = 0;
            stdComm_bIsServer = 0;
            stdComm_dplayIdSelf = 0;
        }
        //DirectPlay_CloseConnection();
        stdComm_dword_8321DC = 0;
    }
}

int stdComm_Open(int idx, wchar_t* pwPassword)
{
    DirectPlay_EnumSessions2();

    stdComm_dword_8321E8 = 0;
    stdComm_dword_8321E0 = 1;
    stdComm_dplayIdSelf = DirectPlay_CreatePlayer(jkPlayer_playerShortName, 0);
    stdComm_bIsServer = 0;
    stdComm_dplayIdSelf = 2; // HACK
    jkGuiMultiplayer_checksumSeed = jkGuiMultiplayer_aEntries[idx].checksumSeed;

    if (!SteamAPI_IsSteamRunning())
        return 0;

	//addrServer.Clear();
	//SteamNetworkingUtils()->SteamNetworkingIPAddr_ParseString(&addrServer, jkGuiMultiplayer_aEntries[idx].ip);
	//addrServer.m_port = DEFAULT_SERVER_PORT;

	stdComm_steamLobbyID.Clear();
	stdComm_steamLobbyID.SetFromUint64(jkGuiMultiplayer_aEntries[idx].guidInstance);

    client.Init();
    return 0;
}

void stdComm_Close()
{
    if (!SteamAPI_IsSteamRunning())
        return;

    if (stdComm_bIsServer)
    {
        server.Shutdown();
    }
    else 
    {
        client.Shutdown();
    }
}

int DirectPlay_SendLobbyMessage(void* pPkt, uint32_t pktLen)
{
    return 1;
}

void DirectPlay_SetSessionDesc(const char* a1, DWORD maxPlayers)
{
    _strncpy(sithDplayGNS_storedEntry.mapJklFname, jkMain_aLevelJklFname, 0x20);
}

BOOL DirectPlay_SetSessionFlagidk(int a1)
{
    return 1;
}

BOOL DirectPlay_Startup()
{
    //IDirectPlay3Vtbl *v0; // esi
    //uint32_t *v1; // eax

    //CoInitialize(0);
    //CoCreateInstance(&rclsid, 0, 1u, &riid, (LPVOID *)&idirectplay);
    jkGuiMultiplayer_numConnections = 0;
    memset(&jkGuiMultiplayer_aConnections, 0, 0x1180u);
    //v0 = idirectplay->lpVtbl;
    //v1 = WinIdk_GetDplayGuid();
    //return v0->EnumConnections(idirectplay, (LPCGUID)v1, (LPDPENUMCONNECTIONSCALLBACK)DirectPlay_EnumConnectionsCallback, 0, 0) >= 0;
    return 1;
}

int DirectPlay_EarlyInit(wchar_t* pwIdk, wchar_t* pwPlayerName)
{
    // This can launch straight into a game? Gaming Zone stuff. 1 and 2 autolaunch an MP game.
    return 0;
}

DPID DirectPlay_CreatePlayer(wchar_t* pwIdk, int idk2)
{
    return 1;
}

void DirectPlay_Close()
{
    
}

int DirectPlay_OpenHost(stdCommSession* pEntry)
{
    sithDplayGNS_storedEntry = *pEntry;

    jkPlayer_maxPlayers = pEntry->maxPlayers; // Hack?

    stdComm_bIsServer = 1;

    if (!SteamAPI_IsSteamRunning())
        return 0;
    server.Init();

    return 0;
}

int DirectPlay_GetSession_passwordidk(stdCommSession* pEntry)
{
    sithDplayGNS_storedEntry = *pEntry;

    jkPlayer_maxPlayers = pEntry->maxPlayers; // Hack?

    return 1;
}

class SteamLobbyList
{
public:
	void ListLobbies()
	{
		if (!m_CallResultLobbyMatchList.IsActive())
		{
			SteamMatchmaking()->AddRequestLobbyListDistanceFilter(k_ELobbyDistanceFilterFar);

			SteamAPICall_t hSteamAPICall = SteamMatchmaking()->RequestLobbyList();
			m_CallResultLobbyMatchList.Set(hSteamAPICall, this, &SteamLobbyList::OnLobbyMatchList);
		}
		SteamAPI_RunCallbacks();
	}

	void OnLobbyMatchList(LobbyMatchList_t* pLobbyMatchList, bool bIOFailure)
	{
		dplay_dword_55D618 = 0;
		sithDplayGNS_numEnumd = 0;
		memset(&sithDplayGNS_storedEntryEnum, 0, sizeof(sithDplayGNS_storedEntryEnum));
		memset(&jkGuiMultiplayer_aEntries[0], 0, sizeof(jkGuiMultiplayer_aEntries[0]));

		for (int i = 0; i < pLobbyMatchList->m_nLobbiesMatching && i < 32; ++i)
		{
			CSteamID lobbyID = SteamMatchmaking()->GetLobbyByIndex(i);
			CSteamID ownerID = SteamMatchmaking()->GetLobbyOwner(lobbyID);
			if(lobbyID.IsValid())// && ownerID.IsValid())
			{
				stdCommSession* pEntry = &jkGuiMultiplayer_aEntries[sithDplayGNS_numEnumd];
				stdString_CharToWchar(pEntry->serverName, SteamMatchmaking()->GetLobbyData(lobbyID, "serverName"), 32);
				strcpy_s(pEntry->episodeGobName, 32, SteamMatchmaking()->GetLobbyData(lobbyID, "episodeGobName"));
				strcpy_s(pEntry->mapJklFname, 32, SteamMatchmaking()->GetLobbyData(lobbyID, "mapJklFname"));
				pEntry->maxRank = std::stoi(SteamMatchmaking()->GetLobbyData(lobbyID, "maxRank"));

				pEntry->field_E0 = 10; // todo: fixme
				pEntry->guidInstance = lobbyID.ConvertToUint64();


				pEntry->maxPlayers = SteamMatchmaking()->GetLobbyMemberLimit(lobbyID);
				pEntry->numPlayers = SteamMatchmaking()->GetNumLobbyMembers(lobbyID);
			//	GUID_idk guidInstance;
			//	int maxPlayers;
			//	int numPlayers;
			//	wchar_t serverName[32];
			//	char episodeGobName[32];
			//	char mapJklFname[32];
			//	wchar_t wPassword[32];
			//	int sessionFlags;
			//	int checksumSeed;
			//	int field_E0;
			//	int multiModeFlags;
			//	int tickRateMs;
			//	int maxRank;

				sithDplayGNS_numEnumd++;
				dplay_dword_55D618 = sithDplayGNS_numEnumd;
			}
		}
	}

	CCallResult<SteamLobbyList, LobbyMatchList_t> m_CallResultLobbyMatchList;
};
SteamLobbyList LobbyList;

int DirectPlay_EnumSessions2()
{
    if (!SteamAPI_IsSteamRunning())
        return 0;
    return 0;
}

int stdComm_EnumSessions(int a, void* b)
{
    if (!SteamAPI_IsSteamRunning())
        return 0;

    Hack_ResetClients();
	LobbyList.ListLobbies();
    
    return 0;
}

void DirectPlay_EnumPlayers(int a)
{
    Hack_ResetClients();
}

int DirectPlay_StartSession(void* a, void* b)
{
    return 1;
}

void DirectPlay_Destroy()
{
    
}

int DirectPlay_GetSessionDesc(stdCommSession* pEntry)
{
    //TODO
    return 1;
}

}

#endif
#include "stdComm.h"

#include "Dss/sithMulti.h"
#include "General/stdString.h"
#include "jk.h"

#include "stdPlatform.h"

stdComm_Friend* DirectPlay_apFriends = NULL;
uint32_t DirectPlay_numFriends = 0;

DPID stdComm_dplayIdSelf = 0;

#ifdef PLATFORM_STEAM
stdCommSession* jkGuiMultiplayer_aEntries = NULL;
#else
stdCommSession jkGuiMultiplayer_aEntries[32];
#endif

int stdComm_Startup()
{
    if (stdComm_bInitted)
        return 0;

#ifdef TARGET_HAS_DPLAY
    DirectPlay_Startup();
#endif

#ifdef PLATFORM_BASICSOCKETS
    stdComm_Basic_Startup();
#endif
#ifdef PLATFORM_GNS
    stdComm_GNS_Startup();
#endif
#ifdef PLATFORM_STEAM
	stdComm_Steam_Startup();
#endif
#ifdef PLATFORM_NOSOCKETS
    stdComm_None_Startup();
#endif
    stdComm_bInitted = 1;

    return 1;
}

void stdComm_Shutdown()
{
    if (stdComm_bInitted)
    {
        DirectPlay_Destroy();
        stdComm_bInitted = 0;
    }
#ifdef PLATFORM_GNS
    stdComm_GNS_Shutdown();
#endif
#ifdef PLATFORM_STEAM
	stdComm_Steam_Shutdown();
#endif
}

int stdComm_EarlyInit()
{
    stdComm_dword_8321F8 = DirectPlay_EarlyInit(stdComm_waIdk, sithMulti_name);
    if (stdComm_dword_8321F8)
    {
		stdPlatform_Printf("stdComm_EarlyInit: joining game %ls", stdComm_waIdk);
        stdComm_dword_8321DC = 1;
        stdComm_dword_8321E0 = 1;
        stdComm_dplayIdSelf = DirectPlay_CreatePlayer(jkPlayer_playerShortName, 0);
        if (stdComm_dplayIdSelf)
        {
            if (stdComm_dword_8321F8 == 1)
            {
                stdComm_bIsServer = 1;
            }
            else if (stdComm_dword_8321F8 == 2)
            {
                stdComm_bIsServer = 0;
            }
        }
        else
        {
            DirectPlay_Close();
			return 0;
        }
    }
    return stdComm_dword_8321F8;
}

HRESULT stdComm_EnumSessions2(void)
{
    return DirectPlay_EnumSessions2();
}

int stdComm_seed_idk(stdCommSession *pEntry)
{
    jkGuiMultiplayer_checksumSeed = (__int64)(_frand() * 4294967300.0);
    pEntry->checksumSeed = jkGuiMultiplayer_checksumSeed;
    pEntry->field_E0 = 10;
    if ( DirectPlay_GetSession_passwordidk(pEntry) )
        return 0;
    DirectPlay_Close();
    return 0x80004005;
}

int stdComm_CreatePlayer(stdCommSession *pEntry)
{
    jkGuiMultiplayer_checksumSeed = (__int64)(_frand() * 4294967300.0);
    pEntry->checksumSeed = jkGuiMultiplayer_checksumSeed;
    pEntry->field_E0 = 10;
    int result = DirectPlay_OpenHost(pEntry);
    if ( !result )
    {
        stdComm_dplayIdSelf = DirectPlay_CreatePlayer(jkPlayer_playerShortName, 0);
        if ( stdComm_dplayIdSelf )
        {
		#ifndef PLATFORM_STEAM
            stdComm_dplayIdSelf = 1; // HACK
		#endif
            stdComm_bIsServer = 1;
            stdComm_dword_8321E0 = 1;
            result = 0;
        }
        else
        {
            DirectPlay_Close();
            result = 0x80004005;
        }
    }
    return result;
}

int stdComm_Recv(sithCogMsg *msg)
{
    sithCogMsg *pMsg; // esi
    int ret; // eax
    int msgBytes; // [esp+4h] [ebp-4h] BYREF

    pMsg = msg;
    msgBytes = 2052;
	DPID playerId = 0;

    _memset(&msg->netMsg.cogMsgId, 0, msgBytes); // Added

    // TODO I got a struct offset wrong.....
    ret = DirectPlay_Receive(&playerId, (void*)&msg->netMsg.cogMsgId, &msgBytes);
    if ( ret != -1 )
    {
        if ( !ret )
        {
            pMsg->netMsg.thingIdx = playerId;
            pMsg->netMsg.msg_size = msgBytes - 4;
            pMsg->netMsg.timeMs = sithTime_curMs;
            return 1;
        }
        if ( (g_submodeFlags & SITH_SUBMODE_JOINING) == 0 )
        {
            if (ret == 2)
            {
                sithMulti_sub_4CA470(playerId);
            }
            else if (ret == DPSYS_DELETEPLAYER && sithNet_isServer )
            {
                sithMulti_SendLeaveJoin(playerId, 1);
                return 0;
            }
        }
    }
    return 0;
}

int stdComm_DoReceive()
{
    int result; // eax
    int v1; // [esp+0h] [ebp-8h] BYREF
    DPID v2; // [esp+4h] [ebp-4h] BYREF

    v1 = 2048;
    do {
        result = DirectPlay_Receive(&v2, (int*)sithComm_netMsgTmp.pktData, &v1);
    }
    while ( result != -1 );
    return result;
}

int stdComm_SendToPlayer(sithCogMsg *msg, DPID sendto_id)
{
    uint32_t v2 = msg->netMsg.msg_size + 4;
    if ( sendto_id != INVALID_DPID )
    {
        int ret = DirectPlay_Send(stdComm_dplayIdSelf, sendto_id, &msg->netMsg.cogMsgId, v2);
        if ( !ret )
            return 0;
        ++stdComm_dword_8321F4;
        stdComm_dword_8321F0 += v2;
        return 1;
    }

    if ( !jkPlayer_maxPlayers )
        return 1;

    
    for (int i = 0; i < jkPlayer_maxPlayers; i++)
    {
        sithPlayerInfo* v5 = &jkPlayer_playerInfos[i];
	#ifdef PLATFORM_STEAM
		if ((v5->flags & SITH_PLAYER_JOINEDGAME) != 0 && v5->net_id != stdComm_dplayIdSelf)
	#else
        if ( !i || ((v5->flags & SITH_PLAYER_JOINEDGAME) != 0 && v5->net_id != stdComm_dplayIdSelf) ) // Added: always allow sending to 0, for dedicated servers' fake player
	#endif
        {
            DirectPlay_Send(stdComm_dplayIdSelf, v5->net_id, &msg->netMsg.cogMsgId, v2);
            ++stdComm_dword_8321F4;
            stdComm_dword_8321F0 += v2;
        }
    }

    return 1;
}

int DirectPlay_EnumPlayersCallback(DPID dpId, DWORD dwPlayerType, LPCDPNAME lpName, DWORD dwFlags, LPVOID lpContext)
{
    if (DirectPlay_numPlayers >= 0x20) return 1;

    _memset(&DirectPlay_aPlayers[DirectPlay_numPlayers], 0, sizeof(sithDplayPlayer));

    if (lpName->lpszShortName)
    {
        _wcsncpy(DirectPlay_aPlayers[DirectPlay_numPlayers].waName, lpName->lpszShortName, 0x1Fu);
        DirectPlay_aPlayers[DirectPlay_numPlayers].waName[31] = 0;
    }

    if (lpName->lpszLongName)
    {
        _wcsncpy(&DirectPlay_aPlayers[DirectPlay_numPlayers].waName[20], lpName->lpszLongName, 0x1Fu);
        DirectPlay_aPlayers[DirectPlay_numPlayers].field_66 = 0;
    }

    DirectPlay_aPlayers[DirectPlay_numPlayers].dpId = dpId;
    DirectPlay_numPlayers++;
    return 1;
}

#ifdef PLATFORM_STEAM
int stdComm_EnumFriends()
{
	return DirectPlay_EnumFriends();
}

int stdComm_Invite(DPID id)
{
	return DirectPlay_Invite(id);
}
#endif

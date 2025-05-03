#include "jkGUIMultiLobby.h"

#ifdef PLATFORM_STEAM

#include "General/Darray.h"
#include "General/stdBitmap.h"
#include "General/stdFont.h"
#include "General/stdStrTable.h"
#include "General/stdFileUtil.h"
#include "Engine/rdMaterial.h" // TODO move stdVBuffer
#include "stdPlatform.h"
#include "jk.h"
#include "Gui/jkGUIRend.h"
#include "Gui/jkGUI.h"
#include "Gui/jkGUIDialog.h"
#include "Gameplay/sithPlayer.h"
#include "World/jkPlayer.h"
#include "Main/jkStrings.h"
#include "Win95/stdDisplay.h"
#include "General/stdString.h"
#include "Dss/sithMulti.h"
#include "Win95/stdComm.h"
#include "Modules/std/std3D.h"

static int listbox_images[2] = { JKGUI_BM_UP_15, JKGUI_BM_DOWN_15 };
static int listbox_images2[2] = { JKGUI_BM_UP_15, JKGUI_BM_DOWN_15 };

Darray jkGuiMultiLobby_playerArray;

wchar_t jkGuiMultiLobby_chatText[256] = {L'\0'};

enum jkGuiMultiLobbyButton_t
{
	GUI_OK = 1,
	GUI_CANCEL = -1,
};

enum jkGuiMultiLobbyElement_t
{
	LOBBY_PLAYER_LIST = 0,
	LOBBY_CHAT_BOX = 1,
	LOBBY_CHAT_TEXTBOX = 2,
	LOBBY_OK = 3,
	LOBBY_CANCEL = 4
};


static jkGuiElement jkGuiMultiLobby_buttons[17] =
{
	/*LOBBY_PLAYER_LIST*/	{ELEMENT_LISTBOX, 1, 0, NULL, 0, {20, 100, 140, 251}, 1, 0, NULL, NULL, NULL, listbox_images, {36, 0, 0, 0, 0, {0, 0, 0, 0}}, 0 ,NULL},
	/*LOBBY_CHAT_BOX*/		{ELEMENT_LISTBOX, 1, 0, NULL, 0, {280, 100, 320, 251}, 1, 0, NULL, NULL, NULL, listbox_images, {36, 0, 0, 0, 0, {0, 0, 0, 0}}, 0 ,NULL},
	/*LOBBY_CHAT_TEXTBOX*/	{ELEMENT_TEXTBOX, 0, 0, NULL, 16, {280, 355, 320, 20}, 1, 0, NULL, NULL, NULL, NULL, {0, 0, 0, 0, 0, {0, 0, 0, 0}}, 0},

	/*LOBBY_OK*/			{ELEMENT_TEXTBUTTON,   GUI_OK,  2, "GUI_OK", 3, {420, 430, 200, 40}, 1, 0, NULL, NULL, NULL, NULL, {0, 0, 0, 0, 0, {0, 0, 0, 0}}, 0},
	/*LOBBY_CANCEL*/		{ELEMENT_TEXTBUTTON,   GUI_CANCEL, 2, "GUI_CANCEL", 3, {20, 430, 200, 40}, 1, 0, NULL, NULL, NULL, NULL, {0, 0, 0, 0, 0, {0, 0, 0, 0}}, 0},

	{ ELEMENT_END, 0, 0, NULL, 0, { 0, 0, 0, 0 }, 0, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 }
};

static jkGuiMenu jkGuiMultiLobby_menu =
{
	jkGuiMultiLobby_buttons, -1, 65535, 65535, 15, NULL, NULL, jkGui_stdBitmaps, jkGui_stdFonts, 0, jkGuiMultiLobby_sub_4188B0, "thermloop01.wav", "thrmlpu2.wav", NULL, NULL, NULL, 0, NULL, NULL
};


int jkGuiMultiLobby_UpdatePlayerList(jkGuiElement* pElement, int minIdk, int maxIdk, int idx)
{
	int v5; // ebp
	stdFileSearch* v7; // edi
	int v9; // eax
	char a2a[32]; // [esp+14h] [ebp-1640h] BYREF
	char a1[32]; // [esp+34h] [ebp-1620h] BYREF
	wchar_t name[32]; // [esp+54h] [ebp-1600h] BYREF
	char path[128]; // [esp+94h] [ebp-15C0h] BYREF
	char fpath[128]; // [esp+114h] [ebp-1540h] BYREF
	sithPlayerInfo playerInfo; // [esp+2A0h] [ebp-13B4h] BYREF

	v5 = 0;
	stdString_WcharToChar(a1, jkPlayer_playerShortName, 31);
	a1[31] = 0;
	jkGuiRend_DarrayFreeEntry(&jkGuiMultiLobby_playerArray);
	pElement->selectedTextEntry = idx;
	for (int i = 0; i < DirectPlay_numPlayers; ++i)
	{
		stdString_WcharToChar(a2a, DirectPlay_aPlayers[i].waName, 0x1Fu);
		a2a[31] = 0;

		jkGuiRend_AddStringEntry(&jkGuiMultiLobby_playerArray, a2a, i);

		++v5;
	}

	jkGuiRend_DarrayReallocStr(&jkGuiMultiLobby_playerArray, 0, 0);
	jkGuiRend_SetClickableString(pElement, &jkGuiMultiLobby_playerArray);
	return v5;
}

int jkGuiMultiLobby_Show(stdCommSession3* pMultiEntry)
{
	int v3; // esi
	int v9; // [esp+10h] [ebp-3DCh]

	jkGuiRend_UpdateAudio();

	jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_LOAD]->palette);
	jkGuiRend_DarrayNewStr(&jkGuiMultiLobby_playerArray, 5, 1);

	DirectPlay_EnumPlayers(0);

	wchar_t* pwMultiplayerCharsStr = jkStrings_GetUniStringWithFallback("GUI_FRIENDS");

	jkGuiMultiLobby_buttons[LOBBY_OK].bIsVisible = stdComm_bIsServer;
	
	jkGuiMultiLobby_chatText[0] = L'\0';
	jkGuiMultiLobby_buttons[LOBBY_CHAT_TEXTBOX].wstr = jkGuiMultiLobby_chatText;
	jkGuiMultiLobby_buttons[LOBBY_CHAT_TEXTBOX].selectedTextEntry = 256;

	do
	{
		jkGuiMultiLobby_UpdatePlayerList(&jkGuiMultiLobby_buttons[LOBBY_PLAYER_LIST], 0, 9, 1);
		//jkGuiMultiFriends_UpdateStatus(&jkGuiMultiFriends_menu, jkGuiMultiFriends_buttons[3].selectedTextEntry);
		v3 = 1;

		jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiMultiLobby_menu, &jkGuiMultiLobby_buttons[LOBBY_OK]);
		jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiMultiLobby_menu, &jkGuiMultiLobby_buttons[LOBBY_CANCEL]);
		v9 = jkGuiRend_DisplayAndReturnClicked(&jkGuiMultiLobby_menu);

		switch (v9)
		{
		case GUI_CANCEL:
			v3 = 0;
			break;

		case GUI_OK:
			v3 = 0;
			break;
	
		default:
			break;
		}
	} while (v3);

	jkGuiRend_DarrayFree(&jkGuiMultiLobby_playerArray);
	jkGui_SetModeGame();
	return v9;
}

void jkGuiMultiLobby_Startup()
{
#ifdef MENU_16BIT
	jkGui_InitMenu(&jkGuiMultiLobby_menu, jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_LOAD], jkGui_stdBitmaps16[JKGUI_BM_BK_BUILD_LOAD]);
#else
	jkGui_InitMenu(&jkGuiMultiLobby_menu, jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_LOAD]);
#endif
}

void jkGuiMultiLobby_Shutdown()
{
#ifdef MENU_16BIT
	jkGuiMultiLobby_menu.bkBm16 = NULL;
#endif
}

void jkGuiMultiLobby_sub_4188B0(jkGuiMenu* pMenu)
{
	

	//uint32_t v1; // ecx
   //
   // if ( jkGuiMultiFriends_idkType )
   // {
   //     v1 = stdPlatform_GetTimeMsec() - jkGuiMultiFriends_msStart;
   //     if ( v1 > SCORE_DELAY_MS )
   //         pMenu->lastClicked = 1;
   //     if ( v1 / 1000 != jkGuiMultiFriends_dword_5568D0 )
   //     {
   //         jkGuiMultiFriends_dword_5568D0 = v1 / 1000;
   //         jk_snwprintf(jkGuiMultiFriends_waTmp, 0x20u, L"%d", 30 - v1 / 1000);
   //         jkGuiMultiFriends_buttons[88].wstr = jkGuiMultiFriends_waTmp;
   //         jkGuiRend_UpdateAndDrawClickable(&jkGuiMultiFriends_buttons[88], pMenu, 1);
   //     }
   // }
}

#endif

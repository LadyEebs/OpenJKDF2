#include "jkGUIMultiPlayers.h"

#include "General/Darray.h"
#include "General/stdString.h"
#include "Dss/sithGamesave.h"
#include "Main/Main.h"
#include "Main/jkMain.h"
#include "Main/jkStrings.h"
#include "Gui/jkGUI.h"
#include "Gui/jkGUIRend.h"
#include "Gui/jkGUIDialog.h"
#include "Gui/jkGUITitle.h"
#include "World/jkPlayer.h"
#include "World/sithWorld.h"
#include "Cog/jkCog.h"
#include "stdPlatform.h"
#include "Win95/stdComm.h"
#include "Dss/sithMulti.h"
#ifdef PLATFORM_STEAM
#include "Modules/sith/Engine/sithVoice.h"
#endif

#include "jk.h"

static int jkGuiMultiPlayers_listIdk[2] = {0xd, 0xe};

Darray jkGuiMultiPlayers_DarrayEntries;
uint32_t jkGuiMultiPlayers_numEntries;
wchar_t jkGuiMultiPlayers_wtextSaveName[256] = { 0 };
wchar_t jkGuiMultiPlayers_wtextHealth[256] = { 0 };
wchar_t jkGuiMultiPlayers_wtextShields[256] = { 0 };
wchar_t jkGuiMultiPlayers_wtextEpisode[256] = { 0 };

enum jkGuiMultiPlayer_Element
{
	GUI_PLAYER_SELECT_TEXT  = 1,
	GUI_PLAYER_LISTBOX      = 2,
	GUI_DONE_BTN            = 3,
	GUI_PLAYER_KICK_BTN     = 4,
	GUI_PLAYER_BAN_BTN      = 5,
	GUI_PLAYER_MUTE_BTN     = 6,
};

static jkGuiElement jkGuiMultiPlayers_aElements[15] = {
    {ELEMENT_TEXT, 0, 5, 0, 3, {0x32, 0x32, 0x1F4, 0x1E}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, "GUIEXT_SELECT_PLAYER", 2, {0x28, 0x69, 0x14A, 0x14}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_LISTBOX, 3039, 0x0, 0, 0, {0x28, 0x82, 0x14A, 0x10F}, 1, 0, 0, 0, jkGuiMultiPlayers_ListClick, jkGuiMultiPlayers_listIdk, {0}, 0},
    {ELEMENT_TEXTBUTTON, -1, 0x2, "GUI_DONE", 3, {0, 0x1AE, 0x0C8, 0x28}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 0, 2, "GUIEXT_KICK_PLAYER", 3, {250, 0x1AE, 0x0B4, 0x28}, 1, 0, 0, 0, jkGuiMultiPlayers_KickClicked, 0, {0}, 0},
	{ELEMENT_TEXTBUTTON, 0, 2, "GUIEXT_BAN_PLAYER", 3, {380, 0x1AE, 0x0B4, 0x28}, 1, 0, 0, 0, jkGuiMultiPlayers_BanClicked, 0, {0}, 0},
	{ELEMENT_TEXTBUTTON, 0, 2, "GUIEXT_MUTE_PLAYER", 3, {380, 0x1AE, 0x0B4, 0x28}, 1, 0, 0, 0, jkGuiMultiPlayers_MuteClicked, 0, {0}, 0},
   {ELEMENT_END, 0, 0, 0, 0, {0}, 0, 0, 0, 0, 0, 0, {0}, 0},
};

// todo: update the player list
static jkGuiMenu jkGuiMultiPlayers_menu = {jkGuiMultiPlayers_aElements, 0xFFFFFFFF, 0xFFFF, 0xFFFF, 0xF, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0};

int jkGuiMultiPlayers_ListClick(jkGuiElement *element, jkGuiMenu *menu, int mouseX, int mouseY, BOOL redraw)
{
    jkGuiRend_ClickSound(element, menu, mouseX, mouseY, redraw);
    if ( redraw )
        return 12345;

	if (sithNet_isMulti)
	{
		jkGuiMultiPlayers_aElements[GUI_PLAYER_KICK_BTN].bIsVisible = sithNet_isServer && DirectPlay_aPlayers[element->selectedTextEntry].dpId != stdComm_dplayIdSelf;
		jkGuiMultiPlayers_aElements[GUI_PLAYER_BAN_BTN].bIsVisible = sithNet_isServer && DirectPlay_aPlayers[element->selectedTextEntry].dpId != stdComm_dplayIdSelf;
		jkGuiMultiPlayers_aElements[GUI_PLAYER_MUTE_BTN].bIsVisible = DirectPlay_aPlayers[element->selectedTextEntry].dpId != stdComm_dplayIdSelf;
	}

    jkGuiMultiPlayers_PopulateInfo(1);
    return 0;
}

void jkGuiMultiPlayers_PopulateInfo(int bRedraw)
{
}

// MOTS altered
void jkGuiMultiPlayers_PopulateList()
{
	DirectPlay_EnumPlayers(0);

    jkGuiRend_DarrayNewStr(&jkGuiMultiPlayers_DarrayEntries, DirectPlay_numPlayers, 1);
    jkGuiMultiPlayers_numEntries = 0;
    
	for (int i = 0; i < DirectPlay_numPlayers; ++i)
		jkGuiRend_DarrayReallocStr(&jkGuiMultiPlayers_DarrayEntries, DirectPlay_aPlayers[i].waName, DirectPlay_aPlayers[i].dpId);

    jkGuiRend_DarrayReallocStr(&jkGuiMultiPlayers_DarrayEntries, 0, 0);
}

int jkGuiMultiPlayers_Show()
{
    jkGuiMultiPlayers_PopulateList();
    jkGuiRend_SetClickableString(&jkGuiMultiPlayers_aElements[GUI_PLAYER_LISTBOX], &jkGuiMultiPlayers_DarrayEntries);

    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiMultiPlayers_menu, &jkGuiMultiPlayers_aElements[GUI_DONE_BTN]);
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiMultiPlayers_menu, &jkGuiMultiPlayers_aElements[GUI_DONE_BTN]);
    jkGuiMultiPlayers_menu.focusedElement = &jkGuiMultiPlayers_aElements[GUI_PLAYER_LISTBOX];
    jkGuiMultiPlayers_PopulateInfo(0);

	jkGuiMultiPlayers_aElements[GUI_PLAYER_KICK_BTN].bIsVisible = sithNet_isMulti && sithNet_isServer && DirectPlay_aPlayers[0].dpId != stdComm_dplayIdSelf;
	jkGuiMultiPlayers_aElements[GUI_PLAYER_MUTE_BTN].bIsVisible = sithNet_isMulti && DirectPlay_aPlayers[0].dpId != stdComm_dplayIdSelf;

	int result;
    while ( 1 )
    {
		result = jkGuiRend_DisplayAndReturnClicked(&jkGuiMultiPlayers_menu);
		switch (result)
		{
		case -1:
			break;	
		default:
			continue;
		}
		break;
    }

    jkGuiRend_DarrayFree(&jkGuiMultiPlayers_DarrayEntries);
	jkGui_SetModeGame();
	jkGuiMultiPlayers_numEntries = 0;
    return result;
}

int jkGuiMultiPlayers_PopulateInfoInit(jkGuiElement *a1, jkGuiMenu *a2, int a3, int a4, BOOL redraw)
{
    jkGuiMultiPlayers_PopulateInfo(1);
    return 0;
}

void jkGuiMultiPlayers_Startup()
{
#ifdef MENU_16BIT
	jkGui_InitMenu(&jkGuiMultiPlayers_menu, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP], jkGui_stdBitmaps16[JKGUI_BM_BK_SETUP]);
#else
    jkGui_InitMenu(&jkGuiMultiPlayers_menu, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);
#endif
}

void jkGuiMultiPlayers_Shutdown()
{
#ifdef MENU_16BIT
	jkGuiMultiPlayers_menu.bkBm16 = NULL;
#endif
    ;
}

int jkGuiMultiPlayers_KickClicked(jkGuiElement* element, jkGuiMenu* menu, int mouseX, int mouseY, int bRedraw)
{
	if (!sithNet_isMulti || !sithNet_isServer)
		return 0;
	
	if (DirectPlay_aPlayers[element->selectedTextEntry].dpId == stdComm_dplayIdSelf)
		return 0;

	wchar_t* wstr_confirmDel = jkStrings_GetUniStringWithFallback("GUIEXT_CONFIRM_KICK_PLAYER");
	wchar_t* wstr_del = jkStrings_GetUniStringWithFallback("GUIEXT_KICK_PLAYER");
#ifdef MENU_16BIT
	jkuGuiRend_dialogBackgroundMenu = &jkGuiMultiPlayers_menu;
#endif
	if (jkGuiDialog_YesNoDialog(wstr_del, wstr_confirmDel))
		sithMulti_SendQuit(DirectPlay_aPlayers[element->selectedTextEntry].dpId);
	
	jkGuiMultiPlayers_PopulateList();

	return 1;
}

int jkGuiMultiPlayers_BanClicked(jkGuiElement* element, jkGuiMenu* menu, int mouseX, int mouseY, int bRedraw)
{
	if (!sithNet_isMulti || !sithNet_isServer)
		return 0;

	if (DirectPlay_aPlayers[element->selectedTextEntry].dpId == stdComm_dplayIdSelf)
		return 0;

	wchar_t* wstr_confirmDel = jkStrings_GetUniStringWithFallback("GUIEXT_CONFIRM_BAN_PLAYER");
	wchar_t* wstr_del = jkStrings_GetUniStringWithFallback("GUIEXT_BAN_PLAYER");
#ifdef MENU_16BIT
	jkuGuiRend_dialogBackgroundMenu = &jkGuiMultiPlayers_menu;
#endif
	if (jkGuiDialog_YesNoDialog(wstr_del, wstr_confirmDel))
		sithMulti_Ban(DirectPlay_aPlayers[element->selectedTextEntry].dpId);

	jkGuiMultiPlayers_PopulateList();
	return 1;
}

int jkGuiMultiPlayers_MuteClicked(jkGuiElement* element, jkGuiMenu* menu, int mouseX, int mouseY, int bRedraw)
{
	if (!sithNet_isMulti || !sithNet_isServer)
		return 0;

	if (DirectPlay_aPlayers[element->selectedTextEntry].dpId == stdComm_dplayIdSelf)
		return 0;

	sithVoice_ToggleChannelMuted(DirectPlay_aPlayers[element->selectedTextEntry].dpId);

	jkGuiMultiPlayers_PopulateList();

	return 1;
}
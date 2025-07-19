#include "jkGUIEsc.h"

#include "General/Darray.h"
#include "General/stdBitmap.h"
#include "General/stdString.h"
#include "General/stdFont.h"
#include "Engine/rdMaterial.h" // TODO move stdVBuffer
#include "stdPlatform.h"
#include "jk.h"
#include "Gui/jkGUIRend.h"
#include "Gui/jkGUI.h"
#include "Gui/jkGUIDialog.h"
#include "Gui/jkGUIObjectives.h"
#include "Gui/jkGUIMap.h"
#include "Gui/jkGUISaveLoad.h"
#include "Gui/jkGUISetup.h"
#include "Gui/jkGUIForce.h"
#include "World/jkPlayer.h"
#include "Main/jk.h"
#include "Main/jkStrings.h"
#include "Main/jkMain.h"
#include "Dss/sithMulti.h"
#ifdef PLATFORM_STEAM
#include "JK/GUI/jkGUIMultiFriends.h"
#include "JK/GUI/jkGUIMultiLobby.h"
#include "JK/GUI/jkGUIMultiPlayers.h"
#endif

enum jkGuiEscButton_t
{
    JKGUIESC_RETURNTOGAME = 1,
    JKGUIESC_OBJECTIVES   = 10,
    JKGUIESC_MAP          = 11,
    JKGUIESC_JEDIPOWERS   = 12,
    JKGUIESC_LOAD         = 13,
    JKGUIESC_RESTART      = 14,
    JKGUIESC_SAVE         = 15,
    JKGUIESC_SETUP        = 16,
    JKGUIESC_ABORT        = 17,
#ifdef PLATFORM_STEAM
	JKGUIESC_PLAYERS      = 18,
	JKGUIESC_INVITE       = 19,
#endif
};

enum jkGuiEscElement_t
{
    JKGUIESC_ELMT_OBJECTIVES   = 0,
    JKGUIESC_ELMT_MAP          = 1,
    JKGUIESC_ELMT_JEDIPOWERS   = 2,
    JKGUIESC_ELMT_RETURNTOGAME = 3,
    JKGUIESC_ELMT_LOAD         = 4,
    JKGUIESC_ELMT_SAVE         = 5,
    JKGUIESC_ELMT_RESTART      = 6,
    JKGUIESC_ELMT_SETUP        = 7,
    JKGUIESC_ELMT_ABORT        = 8,
#ifdef PLATFORM_STEAM
	JKGUIESC_ELMT_PLAYERS      = 9,
	JKGUIESC_ELMT_INVITE       = 10,
#endif
};

static jkGuiElement jkGuiEsc_aElements[] = {
    { ELEMENT_TEXTBUTTON, JKGUIESC_OBJECTIVES,   5, "GUI_OBJECTIVES",     3, {  0, 50,  400, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_MAP,          5, "GUI_MAP",            3, {  0, 100, 400, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_JEDIPOWERS,   5, "GUI_JEDIPOWERS",     3, {  0, 150, 400, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_RETURNTOGAME, 5, "GUI_RETURN_TO_GAME", 3, {  0, 240, 400, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_LOAD,         5, "GUI_LOAD",           3, {400, 270, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_SAVE,         5, "GUI_SAVE",           3, {400, 320, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_RESTART,      5, "GUI_RESTART",        3, {400, 220, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_SETUP,        5, "GUI_SETUP",          3, {400, 370, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_ABORT,        5, "GUI_ABORT",          3, {400, 420, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
#ifdef PLATFORM_STEAM
	{ ELEMENT_TEXTBUTTON, JKGUIESC_PLAYERS,      5, "GUIEXT_PLAYERS",     3, {400, 270, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
	{ ELEMENT_TEXTBUTTON, JKGUIESC_INVITE,       5, "GUIEXT_INVITE",      3, {400, 320, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
#endif
    { ELEMENT_END,        0,                     0,  NULL,                0, {0},                  0,  0,  0,  0,  0,  0, {0}, 0}
};

static jkGuiMenu jkGuiEsc_menu = { jkGuiEsc_aElements, -1, 0x0FFFF, 0x0FFFF, 0x0F, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0 };

void jkGuiEsc_Startup()
{
#ifdef MENU_16BIT
	jkGui_InitMenu(&jkGuiEsc_menu, jkGui_stdBitmaps[JKGUI_BM_BK_ESC], jkGui_stdBitmaps16[JKGUI_BM_BK_ESC]);
#else
    jkGui_InitMenu(&jkGuiEsc_menu, jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
#endif
    jkGuiEsc_bInitialized = 1;
}

void jkGuiEsc_Shutdown()
{
#ifdef MENU_16BIT
	jkGuiEsc_menu.bkBm16 = NULL;
#endif
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__); // Added
    jkGuiEsc_bInitialized = 0;
}

void jkGuiEsc_Show()
{
    int32_t v3; // eax

    if ( sithNet_isMulti )
    {
        jkGuiEsc_aElements[JKGUIESC_ELMT_LOAD].bIsVisible = 0;
        jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 0;
        jkGuiEsc_aElements[JKGUIESC_ELMT_OBJECTIVES].bIsVisible = !!(sithMulti_multiModeFlags & MULTIMODEFLAG_COOP); // Added: co-op
        jkGuiEsc_aElements[JKGUIESC_ELMT_RESTART].bIsVisible = 0;
#ifdef PLATFORM_STEAM
		jkGuiEsc_aElements[JKGUIESC_ELMT_PLAYERS].bIsVisible = 1;
		jkGuiEsc_aElements[JKGUIESC_ELMT_INVITE].bIsVisible = 1;
#endif
    }
    else
    {
        jkGuiEsc_aElements[JKGUIESC_ELMT_LOAD].bIsVisible = 1;
        jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 1;
        jkGuiEsc_aElements[JKGUIESC_ELMT_OBJECTIVES].bIsVisible = 1;
        jkGuiEsc_aElements[JKGUIESC_ELMT_RESTART].bIsVisible = 1;
#ifdef PLATFORM_STEAM
		jkGuiEsc_aElements[JKGUIESC_ELMT_PLAYERS].bIsVisible = 0;
		jkGuiEsc_aElements[JKGUIESC_ELMT_INVITE].bIsVisible = 0;
#endif

        // MOTS added
        if (Main_bMotsCompat) {
            if (sithPlayer_pLocalPlayerThing->thingflags & SITH_TF_DEAD || sithPlayer_pLocalPlayerThing->actorParams.typeflags & SITH_AF_DISABLED)
                jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 0;
            if (sithPlayer_pLocalPlayerThing->actorParams.typeflags & SITH_AF_DISABLED) {
                jkGuiEsc_aElements[JKGUIESC_ELMT_LOAD].bIsVisible = 0;
                jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 0;
                jkGuiEsc_aElements[JKGUIESC_ELMT_RESTART].bIsVisible = 0;
            }
        }
        else {
            if (sithPlayer_pLocalPlayerThing->thingflags & SITH_TF_DEAD)
                jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 0;
        }
    }

    while ( 1 )
    {
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiEsc_menu, &jkGuiEsc_aElements[JKGUIESC_ELMT_RETURNTOGAME]);
        switch (jkGuiRend_DisplayAndReturnClicked(&jkGuiEsc_menu))
        {
            case -1:
                return;

            case JKGUIESC_OBJECTIVES:
                jkGuiObjectives_Show();
                continue;

            case JKGUIESC_MAP:
                jkGuiMap_Show();
                continue;

            case JKGUIESC_JEDIPOWERS:
                jkGuiForce_Show(0, 0.0, 0.0, 0, 0, 0);
                continue;

            case JKGUIESC_LOAD:
                v3 = jkGuiSaveLoad_Show(0);
                if ( v3 == 1 )
                {
                    jkMain_MissionReload();
                    jkGuiRend_UpdateSurface();
                    return;
                }
                if ( v3 != 34 )
                    continue;
                jkGuiRend_UpdateSurface();
                return;

            case JKGUIESC_RESTART:
#ifdef MENU_16BIT
				jkuGuiRend_dialogBackgroundMenu = &jkGuiEsc_menu;
#endif
                if ( jkGuiDialog_YesNoDialog(jkStrings_GetUniStringWithFallback("GUI_RESTART_MISSION"), jkStrings_GetUniStringWithFallback("GUI_CONFIRM_RESTART")) )
                {
					jkPlayer_LoadAutosave();
					jkMain_MissionReload();
					jkGuiRend_UpdateSurface();
				}
                return;

            case JKGUIESC_SAVE:
                if ( jkGuiSaveLoad_Show(1) != 1 )
                    continue;

            case JKGUIESC_RETURNTOGAME:
                jkMain_MissionReload();
                jkGuiRend_UpdateSurface();
                return;

            case JKGUIESC_SETUP:
                jkGuiSetup_Show();
                continue;

            case JKGUIESC_ABORT:
#ifdef MENU_16BIT
				jkuGuiRend_dialogBackgroundMenu = &jkGuiEsc_menu;
#endif
                if ( jkGuiDialog_YesNoDialog(jkStrings_GetUniStringWithFallback("GUI_ABORT_GAME"), jkStrings_GetUniStringWithFallback("GUI_CONFIRM_ABORT")) )
                {
					jkMain_MenuReturn();
					jkGuiRend_UpdateSurface();
				}
				return;

#ifdef PLATFORM_STEAM
			case JKGUIESC_PLAYERS:
				jkGuiMultiPlayers_Show();
				continue;

			case JKGUIESC_INVITE:
				jkGuiMultiFriends_Show(0);
				continue;
#endif

            default:
                continue;
        }
    }
}

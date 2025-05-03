#ifndef _JKGUIMULTILOBBY_H
#define _JKGUIMULTILOBBY_H

#ifdef PLATFORM_STEAM

#include "types.h"

int jkGuiMultiLobby_Show(stdCommSession3* pMultiEntry);

void jkGuiMultiLobby_Startup();
void jkGuiMultiLobby_Shutdown();
void jkGuiMultiLobby_sub_4188B0(jkGuiMenu* pMenu);

#endif

#endif // _JKGUIMULTIFRIENDS_H

#ifndef _JKGUIMULTIPLAYERS_H
#define _JKGUIMULTIPLAYERS_H

#include "types.h"
#include "globals.h"

int jkGuiMultiPlayers_ListClick(jkGuiElement *element, jkGuiMenu *menu, int mouseX, int mouseY, BOOL redraw);
void jkGuiMultiPlayers_PopulateInfo(int bRedraw);

void jkGuiMultiPlayers_PopulateList();
int jkGuiMultiPlayers_Show();
int jkGuiMultiPlayers_PopulateInfoInit(jkGuiElement *a1, jkGuiMenu *a2, int a3, int a4, BOOL redraw);
void jkGuiMultiPlayers_Startup();
void jkGuiMultiPlayers_Shutdown();
int jkGuiMultiPlayers_KickClicked(jkGuiElement* element, jkGuiMenu* menu, int mouseX, int mouseY, int bRedraw);

#endif // _JKGUIMULTIPLAYERS_H

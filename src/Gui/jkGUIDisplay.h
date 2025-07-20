#ifndef _JKGUI_DISPLAY_H
#define _JKGUI_DISPLAY_H

#define jkGuiDisplay_Startup_ADDR (0x00414320)
#define jkGuiDisplay_Shutdown_ADDR (0x004148F0)
#define jkGuiDisplay_PrecalcViewSizes_ADDR (0x004149C0)
#define jkGuiDisplay_Show_ADDR (0x00414A10)
#define jkGuiDisplay_something_d3d_check_related_ADDR (0x00414C60)
#define jkGuiDisplay_UpdateVideoModes_ADDR (0x00414DD0)
#define jkGuiDisplay_UpdateDeviceList_ADDR (0x00414EF0)
#define jkGuiDisplay_sub_415210_ADDR (0x00415210)
#define jkGuiDisplay_sub_4152E0_ADDR (0x004152E0)
#define jkGuiDisplay_InitRenderModeStrings_ADDR (0x00415410)
#define jkGuiDisplay_sub_415620_ADDR (0x00415620)

#if !defined(SDL2_RENDER) && defined(WIN32)
static int (*jkGuiDisplay_Startup)() = (void*)jkGuiDisplay_Startup_ADDR;
static void (*jkGuiDisplay_Shutdown)() = (void*)jkGuiDisplay_Shutdown_ADDR;
static int (*jkGuiDisplay_Show)() = (void*)jkGuiDisplay_Show_ADDR;
static void (*jkGuiDisplay_PrecalcViewSizes)() = (void*)jkGuiDisplay_PrecalcViewSizes_ADDR;
#else
void jkGuiDisplay_Startup();
void jkGuiDisplay_Shutdown();
int jkGuiDisplay_Show();
void jkGuiDisplay_PrecalcViewSizes();
#endif

#endif // _JKGUI_DISPLAY_H

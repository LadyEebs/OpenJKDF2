#include "Gui/jkGUIDisplay.h"

#include "General/stdBitmap.h"
#include "General/stdFont.h"
#include "General/stdString.h"
#include "Engine/rdMaterial.h" // TODO move stdVBuffer
#include "stdPlatform.h"
#include "jk.h"
#include "Gui/jkGUIRend.h"
#include "Gui/jkGUI.h"
#include "Gui/jkGUISetup.h"
#include "World/jkPlayer.h"
#include "Win95/Window.h"
#include "Win95/Windows.h"
#include "Platform/std3D.h"
#include "General/stdMath.h"
#include "Win95/stdDisplay.h"
#include "Platform/wuRegistry.h"
#include "main/jkGame.h"
#include "Main/jkStrings.h"
#include "Gui/jkGUIDialog.h"

#include "jk.h"

#ifdef TILE_SW_RASTER

//#define USE_ORIGINAL

enum jkGuiDecisionButton_t
{
    GUI_GENERAL = 100,
    GUI_GAMEPLAY = 101,
    GUI_DISPLAY = 102,
    GUI_SOUND = 103,
    GUI_CONTROLS = 104,

    GUI_ADVANCED = 200,
};

int jkGuiDisplay_sub_415620(jkGuiElement* param_1, jkGuiMenu* param_2);
int jkGuiDisplay_something_d3d_check_related(jkGuiElement* param_1, jkGuiMenu* param_2, int param_3, int param_4, int param_5);

static int32_t listbox_images[2] = { JKGUI_BM_UP_15, JKGUI_BM_DOWN_15 };
static int32_t slider_images[2] = { JKGUI_BM_SLIDER_BACK_200, JKGUI_BM_SLIDER_THUMB };

static jkGuiElement jkGuiDisplay_buttons2[25] =
{
	{ ELEMENT_TEXT,        0,            0, NULL,                    3, {0, 410, 640, 20},   1, 0, NULL,                        0, 0, 0, {0}, 0},
	{ ELEMENT_TEXT,        0,            6, "GUI_SETUP",             3, {20, 20, 600, 40},   1, 0, NULL,                        0, 0, 0, {0}, 0},
	{ ELEMENT_TEXTBUTTON,  GUI_GENERAL,  2, "GUI_GENERAL",           3, {20, 80, 120, 40},   1, 0, "GUI_GENERAL_HINT",          0, 0, 0, {0}, 0},
	{ ELEMENT_TEXTBUTTON,  GUI_GAMEPLAY, 2, "GUI_GAMEPLAY",          3, {140, 80, 120, 40},  1, 0, "GUI_GAMEPLAY_HINT",         0, 0, 0, {0}, 0},
	{ ELEMENT_TEXTBUTTON,  GUI_DISPLAY,  2, "GUI_DISPLAY",           3, {260, 80, 120, 40},  1, 0, "GUI_DISPLAY_HINT",          0, 0, 0, {0}, 0},
	{ ELEMENT_TEXTBUTTON,  GUI_SOUND,    2, "GUI_SOUND",             3, {380, 80, 120, 40},  1, 0, "GUI_SOUND_HINT",            0, 0, 0, {0}, 0},
	{ ELEMENT_TEXTBUTTON,  GUI_CONTROLS, 2, "GUI_CONTROLS",          3, {500, 80, 120, 40},  1, 0, "GUI_CONTROLS_HINT",         0, 0, 0, {0}, 0},
	{ ELEMENT_CHECKBOX,    0,            0, "GUI_3DACCEL",           0, {70,150,200,40 },    1, 0, NULL,                        0, jkGuiDisplay_something_d3d_check_related, 0, {0}, 0},
	{ ELEMENT_TEXT,        0,            0, "GUI_VIDEOMODES",        3, {20,210,280,20 },    1, 0, NULL,                        0, 0, 0, {0}, 0},																     
	{ ELEMENT_LISTBOX,     0,            0, NULL,                    0, {60, 240, 200, 161}, 1, 0, "GUI_VIDEOMODE_HINT",        0, 0, listbox_images, {0}, 0},
	{ ELEMENT_TEXT,        0,            0, "GUI_BRIGHTNESS",        3, {370, 150, 180,20},  1, 0, NULL,                        0, 0, 0, {0}, 0},
	{ ELEMENT_SLIDER,      0,            0, 9,                       0, {310, 170, 320,30},  1, 0, "GUI_BRIGHTNESS_HINT",       0, 0, slider_images, {0}, 0},
	{ ELEMENT_TEXT,        0,            0, "GUI_MIN",               2, {320, 200, 50, 20},  1, 0, NULL,                        0, 0, 0, {0}, 0},
	{ ELEMENT_TEXT,        0,            0, "GUI_MAX",               2, {590, 200, 50, 20},  1, 0, NULL,                        0, 0, 0, {0}, 0},
	{ ELEMENT_TEXT,        0,            0, "GUI_VIEWSIZE",          3, {370, 230, 180, 20}, 1, 0, NULL,                        0, 0, 0, {0}, 0},	
	{ ELEMENT_SLIDER,      0,            0, 10,                      3, {310, 250, 320, 30}, 1, 0, "GUI_VIEWSIZE_HINT",         0, 0, slider_images, {0}, 0},
	{ ELEMENT_TEXT,        0,            0, "GUI_MIN",               2, {320, 280, 50, 20},  1, 0, NULL,                        0, 0, 0, {0}, 0},
	{ ELEMENT_TEXT,        0,            0, "GUI_MAX",               2, {590, 280, 50, 20},  1, 0, NULL,                        0, 0, 0, {0}, 0},
#ifdef USE_ORIGINAL
	{ ELEMENT_TEXT,        0,            0, "GUI_MINTEXDIM",         2, {310,320,260,20},    1, 0, "GUI_MINTEXDIM_HINT",        0, 0, 0, {0}, 0},
	{ ELEMENT_TEXTBOX,     0,            0, NULL,                    3, {570,320,50,20},     1, 0, NULL,                        0, 0, 0, {0}, 0},
	{ ELEMENT_CHECKBOX,    0,            0, "GUI_BACKBUF_IN_SYSMEM", 0, {340,380,260,20},    1, 0, NULL,                        0, 0, 0, {0}, 0},
#else
	{ ELEMENT_TEXT,        0,            0, NULL,                    3, {0, 0, 0, 0},   0, 0, NULL,                        0, 0, 0, {0}, 0},
	{ ELEMENT_TEXT,        0,            0, NULL,                    3, {0, 0, 0, 0},   0, 0, NULL,                        0, 0, 0, {0}, 0},
	{ ELEMENT_TEXT,        0,            0, NULL,                    3, {0, 0, 0, 0},   0, 0, NULL,                        0, 0, 0, {0}, 0},
#endif
	{ ELEMENT_TEXTBUTTON,  GUI_ADVANCED, 2, "GUI_ADVANCED",          3, {220,430,200,40},    1, 0, "GUI_ADVANCED_HINT",          0, 0, 0, {0}, 0},

	{ ELEMENT_TEXTBUTTON,  1,            2, "GUI_OK",                3, {440, 430, 200, 40}, 1, 0, NULL,                        0, 0, 0, {0}, 0},
	{ ELEMENT_TEXTBUTTON, -1,            2, "GUI_CANCEL",            3, {0, 430, 200, 40},   1, 0, NULL,                        0, 0, 0, {0}, 0},

	{ ELEMENT_END,         0,            0, NULL,                    0, {0},                 0, 0, NULL,                        0, 0, 0, {0}, 0},
};

static jkGuiMenu jkGuiDisplay_menu2 = { jkGuiDisplay_buttons2, 0, 0xFFFF, 0xFFFF, 0x0F, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0 };

static jkGuiElement jkGuiDisplay_buttons[18] = {
	{ ELEMENT_TEXT,        0,            0, NULL,                   3, {0, 410, 640, 20},   1, 0, NULL,                        0, 0,                       0,              {0}, 0},	
	{ ELEMENT_TEXT,        0,            5, "GUI_ADVANCED_TITLE",   3, {10, 10, 620, 40},   1, 0, NULL,                        0, 0,                       0,              {0}, 0},	
	{ ELEMENT_TEXT,        0,            0, "GUI_ADVANCED_WARNING", 3, {10, 50, 620, 40},   1, 0, NULL,                        0, 0,                       0,              {0}, 0},
	{ ELEMENT_TEXT,        0,            0, "GUI_DISPLAYDEVICES",   2, {20, 90, 160, 30},   1, 0, NULL,                        0, 0,                       0,              {0}, 0},
	{ ELEMENT_LISTBOX,     0,            0, NULL,                   0, {20, 122, 600, 51},  1, 0, "GUI_DISPLAYDEVICES_HINT",   0, jkGuiDisplay_sub_415620, listbox_images, {0}, 0},
	{ ELEMENT_TEXT,        0,            0, "GUI_3DDEVICES",        2, {20, 180, 160, 30},  1, 0, NULL,                        0, 0,                       0,              {0}, 0},
	{ ELEMENT_LISTBOX,     0,            0, NULL,                   0, {20,212,600,51},     1, 0, "GUI_3DDEVICES_HINT",        0, 0,                       listbox_images, {0}, 0},
	{ ELEMENT_CHECKBOX,    0,            0, "GUI_CLEAR_DISPLAY",    0, {50, 350, 150, 20},  1, 0, NULL,                        0, 0,                       0,              {0}, 0},
	{ ELEMENT_CHECKBOX,    0,            0, "GUI_DISABLE_PAGE_FLIP",0, {50, 370, 150, 20},  1, 0, NULL,                        0, 0,                       0,              {0}, 0},
	{ ELEMENT_TEXT,        0,            0, "GUI_TEXTURE",          2, {70, 270, 110, 30},  1, 0, NULL,                        0, 0,                       0,              {0}, 0},
	{ ELEMENT_LISTBOX,     0,            0, NULL,                   0, {70, 300, 110, 36},  1, 0, "GUI_TEXTURE_HINT",          0, 0,                       listbox_images, {0}, 0},	
	{ ELEMENT_TEXT,        0,            0, "GUI_GEOMETRY",         2, {260,270,110,30},    1, 0, NULL,                        0, 0,                       0,              {0}, 0},
	{ ELEMENT_LISTBOX,     0,            0, NULL,                   0, {260,300,110,66},    1, 0, "GUI_GEOMETRY_HINT",         0, 0,                       listbox_images, {0}, 0},
	{ ELEMENT_TEXT,        0,            0, "GUI_LIGHTING",         2, {440, 270, 110, 30}, 1, 0, NULL,                        0, 0,                       0,              {0}, 0},
	{ ELEMENT_LISTBOX,     0,            0, NULL,                   0, {440,300,110,66},    1, 0, "GUI_LIGHTING_HINT",         0, 0,                       listbox_images, {0}, 0},

	{ ELEMENT_TEXTBUTTON,  1,            2, "GUI_OK",                3, {440, 430, 200, 40}, 1, 0, NULL,                       0, 0, 0, {0}, 0},
	{ ELEMENT_TEXTBUTTON, -1,            2, "GUI_CANCEL",            3, {0, 430, 200, 40},   1, 0, NULL,                       0, 0, 0, {0}, 0},

	{ ELEMENT_END,         0,            0, NULL,                    0, {0},                 0, 0, NULL,                        0, 0, 0, {0}, 0},
};

static jkGuiMenu jkGuiDisplay_menu = { jkGuiDisplay_buttons, 0, 0xFFFF, 0xFFFF, 0x0F, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0 };

uint32_t jkGuiDisplay_deviceIdx = 0;
uint32_t jkGuiDisplay_deviceIdx3d = 0;
uint32_t jkGuiDisplay_3dDeviceIdx = 0;
stdDisplayEnvironment* jkGuiDisplay_displayEnv = NULL;

Darray jkGuiDisplay_deviceNamesMaybe;
Darray jkGuiDisplay_videoModeList;
Darray jkGuiDisplay_otherDarray;

Darray jkGuiDisplay_texModeStrs;
Darray jkGuiDisplay_geoModeStrs;
Darray jkGuiDisplay_lightModeStrs;

int jkGuiDisplay_has3DAccel = 0;

void jkGuiDisplay_PrecalcViewSizes()
{
	stdVideoDeviceEntry* device = &jkGuiDisplay_displayEnv->devices[Video_modeStruct.modeIdx];
	if (!device->videoModes)
		return;
	stdVideoMode* vidMode = device->videoModes;
	int width = vidMode[Video_modeStruct.descIdx].format.width;
	int height = vidMode[Video_modeStruct.descIdx].format.height;
	jkGame_PrecalcViewSizes(width, height, Video_modeStruct.aViewSizes);
#ifdef TILE_SW_RASTER
	// auto scale HUD
	jkPlayer_hudScale = width < 640 ? 1.0f : (flex_t)width / 640.0f;
#endif
}

void jkGuiDisplay_InitMode()
{
	int highResGraphicsInstalled;
	int closestDeviceIdx;
	int deviceIndex, deviceCount;
	GUID* currentDeviceGuidPtr, * compareGuidPtr;
	stdVideoDeviceEntry* devices;
	char* currentModeGuidBytes, * hwModeGuidBytes;
	int foundMatch = 0;
	stdVideoDeviceEntry* deviceList;
	stdDeviceParams defaultDeviceParams = { 1, 1, 1, 1, 1 };
	render_pair renderSettings = { 0 };
	jkGuiDisplay_displayEnv = stdBuildDisplayEnvironment();

	highResGraphicsInstalled = wuRegistry_GetInt("bHighResGraphicsInstall", 1);

	closestDeviceIdx = stdDisplay_FindClosestDevice(&defaultDeviceParams);
	devices = jkGuiDisplay_displayEnv->devices;

	renderSettings.render_8bpp.palBytes = 0;
	renderSettings.render_rgb.bpp = 8;
	if (highResGraphicsInstalled == 0)
	{
		renderSettings.render_8bpp.width = 320;
		renderSettings.render_8bpp.height = 200;
	}
	else
	{
		renderSettings.render_8bpp.width = 640;
		renderSettings.render_8bpp.height = 480;
	}

	jkGuiDisplay_deviceIdx = closestDeviceIdx;
	Video_modeStruct.modeIdx = closestDeviceIdx;

	Video_modeStruct.descIdx = stdDisplay_FindClosestMode(&renderSettings, &devices[closestDeviceIdx].videoModes, devices[closestDeviceIdx].max_modes);

	//if (devices[closestDeviceIdx].videoModes[Video_modeStruct.descIdx].format.format.bpp != 8)
		//Windows_GameErrorMsgbox("ERR_NEED_256_COLOR");

	Video_modeStruct.viewSizeIdx = 5;

	int loadedDisplayGUID = wuRegistry_GetBytes("displayDeviceGUID", (BYTE*)&Video_modeStruct.deviceGuid, 0x10);
	int loaded3DGUID = wuRegistry_GetBytes("3DDeviceGUID", (BYTE*)&Video_modeStruct.halGuid, 0x10);

	// Load other saved video mode settings from registry with defaults
	Video_modeStruct.descIdx = wuRegistry_GetInt("displayMode", Video_modeStruct.descIdx);
	Video_modeStruct.b3DAccel = wuRegistry_GetBool("b3DAccel", Video_modeStruct.b3DAccel);
	Video_modeStruct.gammaLevel = wuRegistry_GetInt("gammaLevel", Video_modeStruct.gammaLevel);
	Video_modeStruct.minTexSize = wuRegistry_GetInt("minTextureDimension", Video_modeStruct.minTexSize);
	Video_modeStruct.viewSizeIdx = wuRegistry_GetInt("viewSize", Video_modeStruct.viewSizeIdx);
	Video_modeStruct.noPageFlip = wuRegistry_GetBool("bNoPageFlip", Video_modeStruct.noPageFlip);
	Video_modeStruct.sysBackbuffer = wuRegistry_GetBool("bSysBackbuffer", Video_modeStruct.sysBackbuffer);

	if (loaded3DGUID == 0 || loadedDisplayGUID == 0)
	{
		deviceList = jkGuiDisplay_displayEnv->devices;
		Video_modeStruct.deviceGuid = deviceList[Video_modeStruct.modeIdx].device.guid;

	fallback_magic_defaults:
		Video_modeStruct.halGuid.Data4 = 0x5302924;
		Video_modeStruct.halGuid.Data3 = -0x5fff3f69;
		Video_modeStruct.halGuid.Data2 = 0x11d113fc;
		Video_modeStruct.halGuid.Data1 = -0x78fae580;
		Video_modeStruct.Video_8605C8 = 0x45;
		Video_modeStruct.b3DAccel = 0;
	}
	else
	{
		// Search for device matching saved display GUID
		deviceIndex = 0;
		deviceList = jkGuiDisplay_displayEnv->devices;
		devices = deviceList;
		if (jkGuiDisplay_displayEnv->numDevices != 0)
		{
			for (; deviceIndex < jkGuiDisplay_displayEnv->numDevices; ++deviceIndex)
			{
				currentDeviceGuidPtr = &deviceList[deviceIndex].device.guid;

				uint8_t* guidBytes = (uint8_t*)currentDeviceGuidPtr;
				uint8_t* savedGuidBytes = (uint8_t*)&Video_modeStruct.deviceGuid;
				if (memcmp(guidBytes, savedGuidBytes, sizeof(GUID)) == 0)
					break;
			}
		}

		if (deviceIndex >= jkGuiDisplay_displayEnv->numDevices)
		{
			Video_modeStruct.deviceGuid = deviceList[Video_modeStruct.modeIdx].device.guid;
			goto fallback_magic_defaults;
		}

		Video_modeStruct.modeIdx = deviceIndex;

#if 0
		{
			const char* magicGuidBytes = (const char*)currentDeviceGuidPtr;
			const char* saved3DGuidBytes = (const char*)&Video_modeStruct.field_1C;
			int bGuidMatches = 0;
			for (int i = 0; i < 16; i++)
			{
				if (magicGuidBytes[i] != saved3DGuidBytes[i])
				{
					bGuidMatches = 0;
					break;
				}
			}
			if (bGuidMatches)
			{
				Video_modeStruct.Video_8605C8 = 0x45;
			}
			else
			{
				int hwModeCount = devices->halDevices;
				Video_modeStruct.Video_8605C8 = 0;
				if (hwModeCount != 0)
				{
					for (int i = 0; i < jkGuiDisplay_displayEnv->numDevices; ++i)
					{
						currentModeGuidBytes = &deviceList[i].halDevices[i];

						int bGuidMatches = 0;
						uint8_t* guidBytes = (uint8_t*)currentDeviceGuidPtr;
						uint8_t* savedGuidBytes = (uint8_t*)&Video_modeStruct.field_0C;
						for (int i = 0; i < 16; i++)
						{
							if (guidBytes[i] != savedGuidBytes[i])
							{
								bGuidMatches = 0;
								break;
							}
						}
						if (bGuidMatches)
							break;
					}
					currentModeGuidBytes = (char*)(devices->field_2A4 + 0x21c);
					do
					{
						int bModeGuidMatches = 1;
						char* modeGuidPtr = currentModeGuidBytes;
						uint8_t* saved3DGuidPtr = (uint8_t*)&Video_modeStruct.field_1C;
						for (int i = 0; i < 16; i++)
						{
							if ((uint8_t)modeGuidPtr[i] != saved3DGuidPtr[i])
							{
								bModeGuidMatches = 0;
								break;
							}
						}
						if (bModeGuidMatches)
							break;
						currentModeGuidBytes += 0x22c;
						Video_modeStruct.Video_8605C8++;
					} while (Video_modeStruct.Video_8605C8 < hwModeCount);
				}

				if (Video_modeStruct.Video_8605C8 >= hwModeCount)
				{
					Video_modeStruct.field_0C = deviceList[deviceIndex].device.guid.Data1;
					Video_modeStruct.field_10 = deviceList[deviceIndex].device.guid.Data2;
					//Video_modeStruct.field_14 = deviceList[deviceIndex].device.guid.Data3;
					//Video_modeStruct.field_18 = deviceList[deviceIndex].device.guid.Data4;
					goto fallback_magic_defaults;
				}
			}
		}
#endif
	}

	memcpy(&Video_modeStruct2, &Video_modeStruct, sizeof(videoModeStruct));

	deviceCount = jkGuiDisplay_displayEnv->numDevices;
	deviceIndex = 0;
	deviceList = jkGuiDisplay_displayEnv->devices;

	while (deviceIndex < deviceCount && !foundMatch)
	{
		stdVideoDeviceEntry* dev = &deviceList[deviceIndex];

		if (/*dev->device.video_device[0].has3DAccel &&*/
			dev->device.video_device[0].hasNoGuid == 0 &&
			dev->videoModes != NULL)
		{
			stdVideoMode* modeList = dev->videoModes;
			int numModes = dev->max_modes;
			int modeIndex = 0;

			while (modeIndex < numModes && !foundMatch)
			{
				stdVideoMode* mode = &modeList[modeIndex];

				if (mode->format.width != 0 &&
					mode->format.height != 0 &&
					mode->format.texture_size_in_bytes != 0 &&
					//(mode->format.format.unk_40 & 2) != 0 &&
					//(mode->format.format.unk_44 & 0x10) != 0 &&
					(mode->format.format.r_bits != 0 ||
					 mode->format.format.g_bits != 0 ||
					 mode->format.format.b_bits != 0))
				{
					foundMatch = 1;
					if (dev->device.video_device[0].has3DAccel)
						jkGuiDisplay_has3DAccel = 1;
				}
				else
					modeIndex++;
			}
		}
		deviceIndex++;
	}
#if 1
	if (!foundMatch)
	{
		foundMatch = 0;
		deviceIndex = 0;

		while (deviceIndex < deviceCount && !foundMatch)
		{
			stdVideoDeviceEntry* dev = &deviceList[deviceIndex];
			if (/*dev->device.video_device[0].has3DAccel &&*/
				//dev->device.video_device[0].hasNoGuid == 0 &&
				dev->halDevices != NULL)
			{
				stdVideoMode* modeList = dev->halDevices;
				int numModes = dev->field_2A4;
				int modeIndex = 0;

				while (modeIndex < numModes && !foundMatch)
				{
					stdVideoMode* mode = &modeList[modeIndex];

					if (mode->format.width != 0 &&
						mode->format.height != 0 &&
						mode->format.texture_size_in_bytes != 0 &&
						(mode->format.format.unk_40 & 2) != 0 &&
						(mode->format.format.unk_44 & 0x10) != 0 &&
						(mode->format.format.r_bits != 0 ||
						 mode->format.format.g_bits != 0 ||
						 mode->format.format.b_bits != 0))
					{
						foundMatch = 1;
					}
					else
						modeIndex++;
				}
			}
			deviceIndex++;
		}

		if (!foundMatch)
		{
			jkGuiDisplay_has3DAccel = 0;
			goto skip_3D_accel_check;
		}
	}
	jkGuiDisplay_has3DAccel = 1;

skip_3D_accel_check:
#endif
	//if (jkGuiDisplay_has3DAccel == 0)
		Video_modeStruct.b3DAccel = 0;
	//else
	//	Video_modeStruct.b3DAccel = 1;

	stdDisplay_lastDisplayIdx = Video_modeStruct.modeIdx;
	jkGuiDisplay_PrecalcViewSizes();

	//stdDisplay_Open(Video_modeStruct.modeIdx);
	//Video_SetVideoDesc(jkGuiDisplay_menu.palette);
}

void jkGuiDisplay_Startup()
{
	jkGui_InitMenu(&jkGuiDisplay_menu2, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);
	jkGui_InitMenu(&jkGuiDisplay_menu, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);

	// jkGuiDisplay_InitMode()
}

void jkGuiDisplay_Shutdown()
{
	stdFreeDisplayEnvironment(jkGuiDisplay_displayEnv);
	if (!g_should_exit)
	{
		wuRegistry_SaveBytes("displayDeviceGUID", (BYTE*)&Video_modeStruct.deviceGuid, 16);
		wuRegistry_SaveBytes("3DDeviceGUID", (BYTE*)&Video_modeStruct.halGuid, 16);
		wuRegistry_SaveInt("displayMode", Video_modeStruct.descIdx);
		wuRegistry_SaveBool("b3DAccel", Video_modeStruct.b3DAccel);
	}
	wuRegistry_SaveInt("minTextureDimension", Video_modeStruct.minTexSize);
	wuRegistry_SaveInt("viewSize", Video_modeStruct.viewSizeIdx);
	wuRegistry_SaveInt("gammaLevel", Video_modeStruct.gammaLevel);
	wuRegistry_SaveBool("bNoPageFlip", Video_modeStruct.noPageFlip);
	wuRegistry_SaveBool("bSysBackbuffer", Video_modeStruct.sysBackbuffer);
}

void jkGuiDisplay_sub_4152E0(jkGuiElement* element, Darray* darray, videoModeStruct* videoMode)
{
	jkGuiRend_DarrayFreeEntry(darray);

	int modeIdx = videoMode->modeIdx;
	stdVideoDeviceEntry* devices = jkGuiDisplay_displayEnv->devices;

	jkGuiRend_AddStringEntry(darray, "[SW] RenderDroid I", 0x45);
	if (videoMode->Video_8605C8 == 0x45)
		element->selectedTextEntry = 0;

	intptr_t id = 0;
	int entryIdx = 1;

	if (devices[modeIdx].halDevices > 0 && devices[modeIdx].field_2A4)
	{
		d3d_device* devList = devices[modeIdx].halDevices;
		for (int i = 0; i < devices[modeIdx].field_2A4; i++, id++)
		{
			d3d_device* dev = &devList[i];
			//if ((dev->availableBitDepths & DDBD_32) && (dev->hasZBuffer))
			{
				const char* driverType = dev->hasColorModel == 0 ? "SW" : "HW";

				char label[128];
				_snprintf(label, sizeof(label), "[%s] %s", driverType, dev->deviceName);

				jkGuiRend_AddStringEntry(darray, label, id);

				if (videoMode->Video_8605C8 == id)
					element->selectedTextEntry = entryIdx;

				entryIdx++;
			}
		}
	}

	jkGuiRend_DarrayReallocStr(darray, NULL, 0);
	jkGuiRend_SetClickableString(element, darray);
}

int jkGuiDisplay_sub_415620(jkGuiElement* param_1, jkGuiMenu* param_2)
{
	Video_modeStruct.modeIdx = jkGuiDisplay_buttons[4].selectedTextEntry;
	Video_modeStruct.Video_8605C8 = 0;
	jkGuiDisplay_sub_4152E0(&jkGuiDisplay_buttons[6], &jkGuiDisplay_deviceNamesMaybe, &Video_modeStruct);
	Video_modeStruct.descIdx = 0;
	jkGuiRend_UpdateAndDrawClickable(jkGuiDisplay_buttons + 6, param_2, 1);
	return 0;
}

void jkGuiDisplay_UpdateVideoModes(jkGuiElement* guiElement, Darray* stringArray, videoModeStruct* params)
{
	int deviceIndex = params->modeIdx;
	stdDisplayEnvironment* displayEnv = jkGuiDisplay_displayEnv;
	stdVideoDeviceEntry* devices = displayEnv->devices;
	stdVideoMode* videoMode = devices[deviceIndex].videoModes;

	intptr_t modeId;
	int visibleEntryIndex = 0;
	wchar_t modeString[128];

	jkGuiRend_DarrayFreeEntry(stringArray);
	guiElement->selectedTextEntry = 0;

	int maxModes = devices[deviceIndex].max_modes;
	for (modeId = 0; modeId < maxModes; modeId++, videoMode++)
	{
		uint32_t bpp = videoMode->format.format.bpp;

		if (params->b3DAccel)
		{
			// Show 8bpp and 16bpp modes
			//if (bpp == 8 || bpp == 16)
			{
				wchar_t* modeTypeStr = L"";// L"[ModeX]";
				//if (videoMode->field_0 == 0)
				//	modeTypeStr = (wchar_t*)&DAT_0055659c;

				_snwprintf(modeString, sizeof(modeString) / sizeof(wchar_t),
							L"%dx%d %dbpp %s",//%.2gHz",
							videoMode->format.width,
							videoMode->format.height,
							bpp,
							modeTypeStr);//),
							//videoMode->refreshRate);

				jkGuiRend_DarrayReallocStr(stringArray, modeString, modeId);

				if (params->descIdx == modeId)
					guiElement->selectedTextEntry = visibleEntryIndex;

				visibleEntryIndex++;
			}
		}
		else
		{
			// Show only 8bpp modes
			//if (bpp == 8)
			{
				wchar_t* modeTypeStr = L"";// L"[ModeX]";
				//if (videoMode->field_0 == 0)
				//	modeTypeStr = (wchar_t*)&DAT_0055659c;

				_snwprintf(modeString, sizeof(modeString) / sizeof(wchar_t),
							L"%dx%d %dbpp %s",
							videoMode->format.width,
							videoMode->format.height,
							bpp,
							modeTypeStr);

				jkGuiRend_DarrayReallocStr(stringArray, modeString, modeId);

				if (params->descIdx == modeId)
					guiElement->selectedTextEntry = visibleEntryIndex;

				visibleEntryIndex++;
			}
		}
	}

	jkGuiRend_DarrayReallocStr(stringArray, NULL, 0);
	jkGuiRend_SetClickableString(guiElement, stringArray);
}

void jkGuiDisplay_InitRenderModeStrings()
{
	wchar_t* text;
	intptr_t index;

	jkGuiRend_DarrayNewStr(&jkGuiDisplay_geoModeStrs, 4, 1);
	jkGuiRend_DarrayFreeEntry(&jkGuiDisplay_geoModeStrs);

	index = 1;
	text = jkStrings_GetUniString("GUI_GEOMETRY_VERTEX");
	jkGuiRend_DarrayReallocStr(&jkGuiDisplay_geoModeStrs, text, index++);

	text = jkStrings_GetUniString("GUI_GEOMETRY_WIREFRAME");
	jkGuiRend_DarrayReallocStr(&jkGuiDisplay_geoModeStrs, text, index++);

	text = jkStrings_GetUniString("GUI_GEOMETRY_SOLID");
	jkGuiRend_DarrayReallocStr(&jkGuiDisplay_geoModeStrs, text, index++);

	text = jkStrings_GetUniString("GUI_GEOMETRY_TEXTURE");
	jkGuiRend_DarrayReallocStr(&jkGuiDisplay_geoModeStrs, text, index++);

	jkGuiRend_DarrayReallocStr(&jkGuiDisplay_geoModeStrs, NULL, 0);
	jkGuiRend_SetClickableString(&jkGuiDisplay_buttons[12], &jkGuiDisplay_geoModeStrs);
	jkGuiDisplay_buttons[12].selectedTextEntry = Video_modeStruct.geoMode - 1;

	jkGuiRend_DarrayNewStr(&jkGuiDisplay_texModeStrs, 2, 1);
	jkGuiRend_DarrayFreeEntry(&jkGuiDisplay_texModeStrs);

	index = 0;
	text = jkStrings_GetUniString("GUI_TEXTURE_AFFINE");
	jkGuiRend_DarrayReallocStr(&jkGuiDisplay_texModeStrs, text, index++);

	text = jkStrings_GetUniString("GUI_TEXTURE_PERSPECT");
	jkGuiRend_DarrayReallocStr(&jkGuiDisplay_texModeStrs, text, index++);

	jkGuiRend_DarrayReallocStr(&jkGuiDisplay_texModeStrs, NULL, 0);
	jkGuiRend_SetClickableString(&jkGuiDisplay_buttons[10], &jkGuiDisplay_texModeStrs);
	jkGuiDisplay_buttons[10].selectedTextEntry = Video_modeStruct.texMode;

	jkGuiRend_DarrayNewStr(&jkGuiDisplay_lightModeStrs, 4, 1);
	jkGuiRend_DarrayFreeEntry(&jkGuiDisplay_lightModeStrs);

	index = 0;
	text = jkStrings_GetUniString("GUI_LIGHTING_FULL");
	jkGuiRend_DarrayReallocStr(&jkGuiDisplay_lightModeStrs, text, index++);

	text = jkStrings_GetUniString("GUI_LIGHTING_NONE");
	jkGuiRend_DarrayReallocStr(&jkGuiDisplay_lightModeStrs, text, index++);

	text = jkStrings_GetUniString("GUI_LIGHTING_DIFFUSE");
	jkGuiRend_DarrayReallocStr(&jkGuiDisplay_lightModeStrs, text, index++);

	text = jkStrings_GetUniString("GUI_LIGHTING_GOURAUD");
	jkGuiRend_DarrayReallocStr(&jkGuiDisplay_lightModeStrs, text, index++);

	jkGuiRend_DarrayReallocStr(&jkGuiDisplay_lightModeStrs, NULL, 0);
	jkGuiRend_SetClickableString(&jkGuiDisplay_buttons[14], &jkGuiDisplay_lightModeStrs);
	jkGuiDisplay_buttons[14].selectedTextEntry = Video_modeStruct.lightMode;
}


int jkGuidisplay_ShowAdvanced()
{
	int result;
	stdVideoDeviceEntry* devices = jkGuiDisplay_displayEnv->devices;
	int numDevices = jkGuiDisplay_displayEnv->numDevices;
	intptr_t deviceId = 0;
	char entryStr[128];

	jkGuiRend_DarrayNewStr((Darray*)&jkGuiDisplay_otherDarray, numDevices + 1, 1);
	jkGuiRend_DarrayFreeEntry((Darray*)&jkGuiDisplay_otherDarray);

	if (numDevices > 0)
	{
		for (deviceId = 0; deviceId < numDevices; deviceId++)
		{
			stdVideoDeviceEntry* currentDevice = &devices[deviceId];

			const char* accelStr = "3D";
			if (currentDevice->device.video_device[0].has3DAccel == 0)
				accelStr = "Non-3D";

			const char* hwSwStr = "HW";
			if (currentDevice->device.video_device[0].hasGUID == 0)
				hwSwStr = "SW";

			_snprintf(entryStr, sizeof(entryStr), "[%s:%s] %s", hwSwStr, accelStr, currentDevice->device.driverName);

			jkGuiRend_AddStringEntry((Darray*)&jkGuiDisplay_otherDarray, entryStr, deviceId);
		}
	}

	jkGuiRend_DarrayReallocStr((Darray*)&jkGuiDisplay_otherDarray, NULL, 0);
	jkGuiRend_SetClickableString(jkGuiDisplay_buttons + 4, (Darray*)&jkGuiDisplay_otherDarray);

	jkGuiDisplay_buttons[4].selectedTextEntry = Video_modeStruct.modeIdx;
	int lastMode = Video_modeStruct.modeIdx;

	jkGuiRend_DarrayNewStr((Darray*)&jkGuiDisplay_deviceNamesMaybe, 4, 1);
	jkGuiDisplay_sub_4152E0(&jkGuiDisplay_buttons[6], &jkGuiDisplay_deviceNamesMaybe, &Video_modeStruct);
	jkGuiDisplay_InitRenderModeStrings();

	jkGuiDisplay_buttons[7].selectedTextEntry = Video_modeStruct.has3DAccel;
	jkGuiDisplay_buttons[8].selectedTextEntry = Video_modeStruct.noPageFlip;

	jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiDisplay_menu, jkGuiDisplay_buttons + 0xF);
	jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiDisplay_menu, jkGuiDisplay_buttons + 0x10);

	result = jkGuiRend_DisplayAndReturnClicked(&jkGuiDisplay_menu);
	if (result == 1)
	{
		int deviceSelect = jkGuiRend_GetId((Darray*)&jkGuiDisplay_otherDarray, jkGuiDisplay_buttons[4].selectedTextEntry);
		if (deviceSelect != lastMode)
		{
			wchar_t* v4 = jkStrings_GetUniStringWithFallback("GUISOUND_MUSTRESTART");
			wchar_t* v2 = jkStrings_GetUniStringWithFallback("GUISOUND_WARNING");
			if(jkGuiDialog_OkCancelDialog(v2, v4))
			{
				Video_modeStruct.modeIdx = deviceSelect;

				GUID_U* selectedGuid = &jkGuiDisplay_displayEnv->devices[Video_modeStruct.modeIdx].device.guid;
				Video_modeStruct.deviceGuid = *selectedGuid;

				Video_modeStruct.Video_8605C8 =
					jkGuiRend_GetId((Darray*)&jkGuiDisplay_deviceNamesMaybe,
									jkGuiDisplay_buttons[6].selectedTextEntry);

				if (Video_modeStruct.Video_8605C8 == 0x45)
				{
					// Default fallback values
					Video_modeStruct.halGuid.Data1 = -0x78FAE580;
					Video_modeStruct.halGuid.Data2 = 0x5302924;
					Video_modeStruct.halGuid.Data3 = 0x11D113FC;
					Video_modeStruct.halGuid.Data4 = -0x5FFF3F69;
				}
				else
				{
					 // Load fields from device mode data
					//int baseOffset = jkGuiDisplay_displayEnv->devices[Video_modeStruct.modeIdx].field_2A4;
					//int dataOffset = baseOffset + 0x21C + Video_modeStruct.Video_8605C8 * 0x22C;
					//Video_modeStruct.field_1C = *(int*)(dataOffset);
					//Video_modeStruct.field_20 = *(int*)(dataOffset + 4);
					//Video_modeStruct.field_24 = *(int*)(dataOffset + 8);
					//Video_modeStruct.field_28 = *(int*)(dataOffset + 0xC);
				}

				wuRegistry_SaveBytes("displayDeviceGUID", (BYTE*)&Video_modeStruct.deviceGuid, 16);
				wuRegistry_SaveBytes("3DDeviceGUID", (BYTE*)&Video_modeStruct.halGuid, 16);
				wuRegistry_SaveInt("displayMode", Video_modeStruct.descIdx);
				wuRegistry_SaveBool("b3DAccel", Video_modeStruct.b3DAccel);

				g_should_exit = 1;
				openjkdf2_restartMode = OPENJKDF2_RESTART_DEVICE_CHANGE;
			}
		}
	#if 0
		Video_modeStruct.modeIdx = jkGuiRend_GetId((Darray*)&jkGuiDisplay_otherDarray, jkGuiDisplay_buttons[4].selectedTextEntry);

		GUID* selectedGuid = &jkGuiDisplay_displayEnv->devices[Video_modeStruct.modeIdx].device.guid;
		Video_modeStruct.field_0C = selectedGuid->Data1;
		Video_modeStruct.field_10 = selectedGuid->Data2;
		Video_modeStruct.field_14 = selectedGuid->Data3;
		Video_modeStruct.field_18 = selectedGuid->Data4;

		Video_modeStruct.Video_8605C8 =
			jkGuiRend_GetId((Darray*)&jkGuiDisplay_deviceNamesMaybe,
							jkGuiDisplay_buttons[6].selectedTextEntry);

		if (Video_modeStruct.Video_8605C8 == 0x45)
		{
			// Default fallback values
			Video_modeStruct.field_1C = -0x78FAE580;
			Video_modeStruct.field_28 = 0x5302924;
			Video_modeStruct.field_20 = 0x11D113FC;
			Video_modeStruct.field_24 = -0x5FFF3F69;
		}
		else
		{
			 // Load fields from device mode data
			//int baseOffset = jkGuiDisplay_displayEnv->devices[Video_modeStruct.modeIdx].field_2A4;
			//int dataOffset = baseOffset + 0x21C + Video_modeStruct.Video_8605C8 * 0x22C;
			//Video_modeStruct.field_1C = *(int*)(dataOffset);
			//Video_modeStruct.field_20 = *(int*)(dataOffset + 4);
			//Video_modeStruct.field_24 = *(int*)(dataOffset + 8);
			//Video_modeStruct.field_28 = *(int*)(dataOffset + 0xC);
		}
#endif

		Video_modeStruct.has3DAccel = jkGuiDisplay_buttons[7].selectedTextEntry;
		Video_modeStruct.noPageFlip = jkGuiDisplay_buttons[8].selectedTextEntry;

		Video_modeStruct.texMode =
			jkGuiRend_GetId((Darray*)&jkGuiDisplay_texModeStrs,
							jkGuiDisplay_buttons[10].selectedTextEntry);

		Video_modeStruct.geoMode =
			jkGuiRend_GetId((Darray*)&jkGuiDisplay_geoModeStrs,
							jkGuiDisplay_buttons[12].selectedTextEntry);

		Video_modeStruct.lightMode =
			jkGuiRend_GetId((Darray*)&jkGuiDisplay_lightModeStrs,
							jkGuiDisplay_buttons[14].selectedTextEntry);

		Video_modeStruct.b3DAccel = (Video_modeStruct.Video_8605C8 != 0x45);
	}

	jkGuiRend_DarrayFree((Darray*)&jkGuiDisplay_otherDarray);
	jkGuiRend_DarrayFree((Darray*)&jkGuiDisplay_deviceNamesMaybe);
	jkGuiRend_DarrayFree((Darray*)&jkGuiDisplay_geoModeStrs);
	jkGuiRend_DarrayFree((Darray*)&jkGuiDisplay_texModeStrs);
	jkGuiRend_DarrayFree((Darray*)&jkGuiDisplay_lightModeStrs);

	return result;
}

int jkGuiDisplay_Show()
{
	stdVideoMode* videoModes;
	int retVal;
	int loopCounter;
	videoModeStruct* modeStructPtr;
	int* modeIndicesPtr;
	wchar_t minTexSizeStr[16] = { 0 };  // zero-initialized wide char string buffer

	int savedModeIndices[65];
	modeStructPtr = &Video_modeStruct;
	modeIndicesPtr = savedModeIndices;
	for (int i = 65; i > 0; i--)
	{
		*modeIndicesPtr++ = modeStructPtr->modeIdx;
		modeStructPtr = (videoModeStruct*)&modeStructPtr->descIdx;
	}

	jkGuiRend_DarrayNewStr((Darray*)&jkGuiDisplay_videoModeList, 0x10, 1);

	jkGui_sub_412E20(&jkGuiDisplay_menu2, 0x66, 0x6b, 0x66);

	jkGuiDisplay_buttons2[21].bIsVisible = 1;//Main_bDisplayConfig;
#ifdef USE_ORIGINAL
	jkGuiDisplay_buttons2[20].selectedTextEntry = Video_modeStruct.sysBackbuffer;

	// Format min texture size string
	_snwprintf(minTexSizeStr, 16, L"%d", Video_modeStruct.minTexSize);

	jkGuiDisplay_buttons2[19].str = (char*)minTexSizeStr;
	jkGuiDisplay_buttons2[19].selectedTextEntry = 32;
#endif

	do
	{
		jkGuiDisplay_UpdateVideoModes(&jkGuiDisplay_buttons2[9], &jkGuiDisplay_videoModeList, &Video_modeStruct);

		// Update some button selections and visibility based on Video_modeStruct or globals
		jkGuiDisplay_buttons2[7].selectedTextEntry = Video_modeStruct.b3DAccel;
		jkGuiDisplay_buttons2[7].bIsVisible = jkGuiDisplay_has3DAccel;
		jkGuiDisplay_buttons2[11].selectedTextEntry = Video_modeStruct.gammaLevel;
		jkGuiDisplay_buttons2[15].otherDataPtr = Video_modeStruct.viewSizeIdx;

		jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiDisplay_menu2, &jkGuiDisplay_buttons2[22]);
		jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiDisplay_menu2, &jkGuiDisplay_buttons2[23]);

		jkGuiSetup_sub_412EF0(&jkGuiDisplay_menu2, 0);

		retVal = jkGuiRend_DisplayAndReturnClicked(&jkGuiDisplay_menu2);
		if (retVal == -1)
		{
			modeIndicesPtr = savedModeIndices;
			modeStructPtr = &Video_modeStruct;
			for (loopCounter = 65; loopCounter > 0; loopCounter--)
			{
				modeStructPtr->modeIdx = *modeIndicesPtr++;
				modeStructPtr = (videoModeStruct*)&modeStructPtr->descIdx;
			}
		}
		else if (retVal == 1)
		{
			Video_modeStruct.b3DAccel = jkGuiDisplay_buttons2[7].selectedTextEntry;
			Video_modeStruct.descIdx = jkGuiRend_GetId((Darray*)&jkGuiDisplay_videoModeList,
													   jkGuiDisplay_buttons2[9].selectedTextEntry);
			Video_modeStruct.gammaLevel = jkGuiDisplay_buttons2[11].selectedTextEntry;
			Video_modeStruct.viewSizeIdx = jkGuiDisplay_buttons2[15].otherDataPtr;
			Video_modeStruct.sysBackbuffer = jkGuiDisplay_buttons2[20].selectedTextEntry;

#ifdef USE_ORIGINAL
			Video_modeStruct.minTexSize = 0;///FUN_005133e0(minTexSizeStr, (void*)local_148, 10);
#endif

			jkGuiDisplay_PrecalcViewSizes();
		}
		else if (retVal == 200)
		{
			Video_modeStruct.descIdx = jkGuiRend_GetId((Darray*)&jkGuiDisplay_videoModeList, jkGuiDisplay_buttons2[9].selectedTextEntry);
			jkGuidisplay_ShowAdvanced();
			jkGuiDisplay_UpdateVideoModes(&jkGuiDisplay_buttons[6], &jkGuiDisplay_videoModeList, &Video_modeStruct);
		}
		else
		{
			break;
		}
	} while (retVal == 200);

	jkGuiRend_DarrayFree((Darray*)&jkGuiDisplay_videoModeList);

	return retVal;
}

int jkGuiDisplay_something_d3d_check_related(jkGuiElement* param_1, jkGuiMenu* param_2, int param_3, int param_4, int param_5)
{
	int iVar1;
	stdVideoDeviceEntry* psVar2;
	int bVar3;

	jkGuiRend_DrawClickableAndUpdatebool(param_1, param_2, param_3, param_4, param_5);
	if (jkGuiDisplay_has3DAccel == 0)
	{
		param_1->selectedTextEntry = 0;
	}
	bVar3 = param_1->selectedTextEntry == 0;
	if (bVar3)
	{
		Video_modeStruct.modeIdx = jkGuiDisplay_deviceIdx;
		Video_modeStruct.Video_8605C8 = 0x45;
	}
	else
	{
		Video_modeStruct.modeIdx = jkGuiDisplay_deviceIdx3d;
		Video_modeStruct.Video_8605C8 = jkGuiDisplay_3dDeviceIdx;
	}
	Video_modeStruct.b3DAccel = !bVar3;// (HKEY)!bVar3;
	Video_modeStruct.descIdx = 0;
	jkGuiDisplay_UpdateVideoModes(&jkGuiDisplay_buttons[6], &jkGuiDisplay_videoModeList, &Video_modeStruct);
	jkGuiRend_UpdateAndDrawClickable(jkGuiDisplay_buttons2 + 9, param_2, 1);
	psVar2 = jkGuiDisplay_displayEnv->devices;
	Video_modeStruct.halGuid = psVar2[Video_modeStruct.modeIdx].device.guid;
	if (Video_modeStruct.Video_8605C8 == 0x45)
	{
		Video_modeStruct.halGuid.Data1 = -0x78FAE580;
		Video_modeStruct.halGuid.Data2 = 0x5302924;
		Video_modeStruct.halGuid.Data3 = 0x11D113FC;
		Video_modeStruct.halGuid.Data4 = -0x5FFF3F69;
		return 0;
	}
	// todo: wtf is this
	//iVar1 = psVar2[Video_modeStruct.modeIdx].field_2A4 + 0x21c + Video_modeStruct.Video_8605C8 * 0x22c;
	//Video_modeStruct.field_1C =	*(int*)(psVar2[Video_modeStruct.modeIdx].field_2A4 + 0x21c + Video_modeStruct.Video_8605C8 * 0x22c);
	//Video_modeStruct.field_20 = *(int*)(iVar1 + 4);
	//Video_modeStruct.field_24 = *(int*)(iVar1 + 8);
	//Video_modeStruct.field_28 = *(int*)(iVar1 + 0xc);
	return 0;
}


#endif

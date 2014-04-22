#include "pluginmain.h"
#include "test.h"

#define plugin_name "testplugin"
#define plugin_version 001
#define plugin_sdkversion 1

int pluginHandle;
HWND hwndDlg;
int hMenu;

DLL_EXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{
    initStruct->pluginVersion=plugin_version;
    initStruct->sdkVersion=plugin_sdkversion;
    strcpy(initStruct->pluginName, plugin_name);
    pluginHandle=initStruct->pluginHandle;
    testInit(initStruct);
    return true;
}

DLL_EXPORT bool plugstop()
{
    testStop();
    return true;
}

DLL_EXPORT void plugsetup(PLUG_SETUPSTRUCT* setupStruct)
{
    hwndDlg=setupStruct->hwndDlg;
    hMenu=setupStruct->hMenu;
    testSetup();
}

extern "C" DLL_EXPORT BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    return TRUE;
}

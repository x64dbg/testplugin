#include "test.h"
#include "pluginsdk\TitanEngine\TitanEngine.h"
#include <windows.h>
#include <stdio.h>
#include <psapi.h>

static void cbInitDebug(CBTYPE cbType, void* callbackInfo)
{
    PLUG_CB_INITDEBUG* info=(PLUG_CB_INITDEBUG*)callbackInfo;
    _plugin_logprintf("[TEST] debugging of file %s started!\n", (const char*)info->szFileName);
}

static void cbStopDebug(CBTYPE cbType, void* callbackInfo)
{
    _plugin_logputs("[TEST] debugging stopped!");
}

static void cbMenuEntry(CBTYPE cbType, void* callbackInfo)
{
    PLUG_CB_MENUENTRY* info=(PLUG_CB_MENUENTRY*)callbackInfo;
    switch(info->hEntry)
    {
    case MENU_DUMP:
        DbgCmdExec("DumpProcess");
        break;
    case MENU_TEST:
        MessageBoxA(hwndDlg, "Test menu entry", "test", MB_ICONINFORMATION);
        break;
    }
}

static bool cbTestCommand(int argc, char* argv[])
{
    _plugin_logputs("[TEST] test command!");
    return true;
}

static bool SaveFileDialog(char Buffer[MAX_PATH])
{
    OPENFILENAMEA sSaveFileName = {0};
    const char szFilterString[] = "Executables (*.exe, *.dll)\0*.exe;*.dll\0All Files (*.*)\0*.*\0\0";
    const char szDialogTitle[] = "Select dump file...";
    sSaveFileName.lStructSize = sizeof(sSaveFileName);
    sSaveFileName.lpstrFilter = szFilterString;
    char File[MAX_PATH]="";
    strcpy(File, Buffer);
    sSaveFileName.lpstrFile = File;
    sSaveFileName.nMaxFile = MAX_PATH;
    sSaveFileName.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    sSaveFileName.lpstrTitle = szDialogTitle;
    sSaveFileName.hwndOwner = GuiGetWindowHandle();
    return (TRUE == GetSaveFileNameA(&sSaveFileName));
}

//DumpProcess [EntryPointVA]
static bool cbDumpProcessCommand(int argc, char* argv[])
{
    duint entry;
    if(argc<2)
        entry=GetContextData(UE_CIP);
    else
        entry=DbgValFromString(argv[1]);
    char mod[MAX_MODULE_SIZE]="";
    if(!DbgGetModuleAt(entry, mod))
    {
        _plugin_logprintf("[TEST] no module at %p...\n", entry);
        return false;
    }
    duint base=DbgModBaseFromName(mod);
    if(!base)
    {
        _plugin_logputs("[TEST] could not get module base...");
        return false;
    }
    HANDLE hProcess=((PROCESS_INFORMATION*)GetProcessInformation())->hProcess;
    if(!GetModuleBaseNameA(hProcess, (HMODULE)base, mod, MAX_MODULE_SIZE))
    {
        _plugin_logputs("[TEST] could not get module base name...");
        return false;
    }
    char szFileName[MAX_PATH]="";
    int len=strlen(mod);
    while(mod[len]!='.' && len)
        len--;
    char ext[MAX_PATH]="";
    if(len)
    {
        strcpy(ext, mod+len);
        mod[len]=0;
    }
    sprintf(szFileName, "%s_dump%s", mod, ext);
    if(!SaveFileDialog(szFileName))
        return true;
    if(!DumpProcess(hProcess, (void*)base, szFileName, entry))
    {
        _plugin_logputs("[TEST] DumpProcess failed...");
        return false;
    }
    _plugin_logputs("[TEST] Dumping done!");
    return true;
}

void testInit(PLUG_INITSTRUCT* initStruct)
{
    _plugin_logprintf("[TEST] pluginHandle: %d\n", pluginHandle);
    _plugin_registercallback(pluginHandle, CB_INITDEBUG, cbInitDebug);
    _plugin_registercallback(pluginHandle, CB_STOPDEBUG, cbStopDebug);
    _plugin_registercallback(pluginHandle, CB_MENUENTRY, cbMenuEntry);
    if(!_plugin_registercommand(pluginHandle, "plugin1", cbTestCommand, false))
        _plugin_logputs("[TEST] error registering the \"plugin1\" command!");
    if(!_plugin_registercommand(pluginHandle, "DumpProcess", cbDumpProcessCommand, true))
        _plugin_logputs("[TEST] error registering the \"DumpProcess\" command!");
}

void testStop()
{
    _plugin_unregistercallback(pluginHandle, CB_INITDEBUG);
    _plugin_unregistercallback(pluginHandle, CB_STOPDEBUG);
    _plugin_unregistercallback(pluginHandle, CB_MENUENTRY);
    _plugin_unregistercommand(pluginHandle, "plugin1");
    _plugin_unregistercommand(pluginHandle, "DumpProcess");
    _plugin_menuclear(hMenu);
}

void testSetup()
{
    _plugin_menuaddentry(hMenu, MENU_TEST, "&Test");
    _plugin_menuaddentry(hMenu, MENU_DUMP, "&DumpProcess");
}
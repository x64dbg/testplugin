#include "test.h"
#include "pluginsdk\TitanEngine\TitanEngine.h"
#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include "FunctionGraph.h"

extern "C" __declspec(dllexport) void CBINITDEBUG(CBTYPE cbType, PLUG_CB_INITDEBUG* info)
{
    _plugin_logprintf("[TEST] debugging of file %s started!\n", (const char*)info->szFileName);
}

extern "C" __declspec(dllexport) void CBSTOPDEBUG(CBTYPE cbType, PLUG_CB_STOPDEBUG* info)
{
    _plugin_logputs("[TEST] debugging stopped!");
}

extern "C" __declspec(dllexport) void CBMENUENTRY(CBTYPE cbType, PLUG_CB_MENUENTRY* info)
{
    switch(info->hEntry)
    {
    case MENU_DUMP:
        if(!DbgIsDebugging())
        {
            _plugin_logputs("you need to be debugging to use this command");
            break;
        }
        DbgCmdExec("DumpProcess");
        break;

    case MENU_TEST:
        MessageBoxA(hwndDlg, "Test Menu Entry Clicked!", "Test Plugin", MB_ICONINFORMATION);
        break;

    case MENU_SELECTION:
    {
        if(!DbgIsDebugging())
        {
            _plugin_logputs("you need to be debugging to use this command");
            break;
        }
        SELECTIONDATA sel;
        char msg[256] = "";
        GuiSelectionGet(GUI_DISASSEMBLY, &sel);
        sprintf(msg, "%p - %p", sel.start, sel.end);
        MessageBoxA(hwndDlg, msg, "Disassembly", MB_ICONINFORMATION);
        sel.start += 4; //expand selection
        GuiSelectionSet(GUI_DISASSEMBLY, &sel);

        GuiSelectionGet(GUI_DUMP, &sel);
        sprintf(msg, "%p - %p", sel.start, sel.end);
        MessageBoxA(hwndDlg, msg, "Dump", MB_ICONINFORMATION);
        sel.start += 4; //expand selection
        GuiSelectionSet(GUI_DUMP, &sel);

        GuiSelectionGet(GUI_STACK, &sel);
        sprintf(msg, "%p - %p", sel.start, sel.end);
        MessageBoxA(hwndDlg, msg, "Stack", MB_ICONINFORMATION);
        sel.start += 4; //expand selection
        GuiSelectionSet(GUI_STACK, &sel);
    }
    break;

    case MENU_FILEOFFSET:
    {
        if(!DbgIsDebugging())
        {
            _plugin_logputs("you need to be debugging to use this command");
            break;
        }
        SELECTIONDATA sel;
        GuiSelectionGet(GUI_DISASSEMBLY, &sel);
        char modpath[MAX_PATH] = "";
        DbgFunctions()->ModPathFromAddr(sel.start, modpath, MAX_PATH);
        HANDLE FileHandle;
        DWORD LoadedSize;
        HANDLE FileMap;
        ULONG_PTR FileMapVA;
        char title[256] = "";
        char modname[MAX_MODULE_SIZE] = "";
        DbgFunctions()->ModNameFromAddr(sel.start, modname, true);
        sprintf(title, "Enter offset in %s", modname);
        char line[GUI_MAX_LINE_SIZE] = "";
        if(!GuiGetLineWindow(title, line))
            break;
        if(!DbgIsValidExpression(line))
        {
            _plugin_logputs("invalid expression entered!");
            break;
        }
        ULONGLONG offset = DbgValFromString(line);
        if(StaticFileLoad(modpath, UE_ACCESS_READ, false, &FileHandle, &LoadedSize, &FileMap, &FileMapVA))
        {
            ULONGLONG rva = ConvertFileOffsetToVA(FileMapVA, //FileMapVA
                                                  FileMapVA + (ULONG_PTR)offset, //Offset inside FileMapVA
                                                  false); //Return without ImageBase
            StaticFileUnload(modpath, true, FileHandle, LoadedSize, FileMap, FileMapVA);
            _plugin_logprintf("Offset %llX has RVA %llX in module %s\n", offset, rva, modname);
            sprintf(line, "disasm %p", rva + DbgModBaseFromName(modname));
            DbgCmdExec(line);
        }
        else
            _plugin_logputs("StaticFileLoad failed :(");
    }
    break;
    }
}

extern "C" __declspec(dllexport) void CBDEBUGEVENT(CBTYPE cbType, PLUG_CB_DEBUGEVENT* info)
{
    if(info->DebugEvent->dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
    {
        //_plugin_logprintf("[TEST] DebugEvent->EXCEPTION_DEBUG_EVENT->%.8X\n", info->DebugEvent->u.Exception.ExceptionRecord.ExceptionCode);
    }
}

static bool cbTestCommand(int argc, char* argv[])
{
    _plugin_logputs("[TEST] test command!");
    char line[GUI_MAX_LINE_SIZE] = "";
    if(!GuiGetLineWindow("test", line))
        _plugin_logputs("[TEST] cancel pressed!");
    else
        _plugin_logprintf("[TEST] line: \"%s\"\n", line);
    return true;
}

static bool SaveFileDialog(char Buffer[MAX_PATH])
{
    OPENFILENAMEA sSaveFileName = {0};
    const char szFilterString[] = "Executables (*.exe, *.dll)\0*.exe;*.dll\0All Files (*.*)\0*.*\0\0";
    const char szDialogTitle[] = "Select dump file...";
    sSaveFileName.lStructSize = sizeof(sSaveFileName);
    sSaveFileName.lpstrFilter = szFilterString;
    char File[MAX_PATH] = "";
    strcpy(File, Buffer);
    sSaveFileName.lpstrFile = File;
    sSaveFileName.nMaxFile = MAX_PATH;
    sSaveFileName.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    sSaveFileName.lpstrTitle = szDialogTitle;
    sSaveFileName.hwndOwner = GuiGetWindowHandle();
    return (FALSE != GetSaveFileNameA(&sSaveFileName));
}

//DumpProcess [EntryPointVA]
static bool cbDumpProcessCommand(int argc, char* argv[])
{
    duint entry;
    if(argc < 2)
        entry = GetContextData(UE_CIP);
    else
        entry = DbgValFromString(argv[1]);
    char mod[MAX_MODULE_SIZE] = "";
    if(!DbgGetModuleAt(entry, mod))
    {
        _plugin_logprintf("[TEST] no module at %p...\n", entry);
        return false;
    }
    duint base = DbgModBaseFromName(mod);
    if(!base)
    {
        _plugin_logputs("[TEST] could not get module base...");
        return false;
    }
    HANDLE hProcess = ((PROCESS_INFORMATION*)TitanGetProcessInformation())->hProcess;
    if(!GetModuleBaseNameA(hProcess, (HMODULE)base, mod, MAX_MODULE_SIZE))
    {
        _plugin_logputs("[TEST] could not get module base name...");
        return false;
    }
    char szFileName[MAX_PATH] = "";
    size_t len = strlen(mod);
    while(mod[len] != '.' && len)
        len--;
    char ext[MAX_PATH] = "";
    if(len)
    {
        strcpy(ext, mod + len);
        mod[len] = 0;
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

//grs addr
static bool cbGrs(int argc, char* argv[])
{
    //Original tool "GetRelocSize" by Killboy/SND
    if(argc < 2)
    {
        _plugin_logputs("[TEST] not enough arguments!");
        return false;
    }
    duint RelocDirAddr = DbgValFromString(argv[1]);
    duint RelocSize = 0;
    IMAGE_RELOCATION RelocDir;
    do
    {
        if(!DbgMemRead(RelocDirAddr, (unsigned char*)&RelocDir, sizeof(IMAGE_RELOCATION)))
        {
            _plugin_logputs("[TEST] invalid relocation table!");
            return false;
        }
        if(!RelocDir.SymbolTableIndex)
            break;
        RelocSize += RelocDir.SymbolTableIndex;
        RelocDirAddr += RelocDir.SymbolTableIndex;
    }
    while(RelocDir.VirtualAddress);

    if(!RelocSize)
    {
        _plugin_logputs("[TEST] invalid relocation table!");
        return false;
    }

    DbgValToString("$result", RelocSize);
    DbgCmdExec("$result");

    return true;
}

static ULONG_PTR GetInstrInfo(ULONG_PTR addr, instr_info* info)
{
    info->addr=addr;
    char comment[MAX_COMMENT_SIZE]="";
    DbgGetCommentAt(addr, comment);
    info->comment=comment;
    BASIC_INSTRUCTION_INFO basicinfo;
    DbgDisasmFastAt(addr, &basicinfo);
    info->instrText=basicinfo.instruction;
    info->jmpaddr=basicinfo.branch && !basicinfo.call ? basicinfo.addr : 0;
    return addr+basicinfo.size;
}

//graph start,end
bool cbGraph(int argc, char* argv[])
{
    if(argc<3)
    {
        _plugin_logputs("[TEST] not enough arguments!");
        return false;
    }
    duint start=DbgValFromString(argv[1]);
    duint end=DbgValFromString(argv[2]);
    if(!start || !end || end < start)
    {
        _plugin_logputs("[TEST] invalid arguments!");
        return false;
    }
    if(!make_flowchart(start, end, L"C:\\test.graph", GetInstrInfo))
    {
        _plugin_logputs("[TEST] failed to generate graph!");
        return false;
    }
    _plugin_logputs("[TEST] graph generated!");
    return true;
}

void testInit(PLUG_INITSTRUCT* initStruct)
{
    _plugin_logprintf("[TEST] pluginHandle: %d\n", pluginHandle);
    if(!_plugin_registercommand(pluginHandle, "plugin1", cbTestCommand, false))
        _plugin_logputs("[TEST] error registering the \"plugin1\" command!");
    if(!_plugin_registercommand(pluginHandle, "DumpProcess", cbDumpProcessCommand, true))
        _plugin_logputs("[TEST] error registering the \"DumpProcess\" command!");
    if(!_plugin_registercommand(pluginHandle, "grs", cbGrs, true))
        _plugin_logputs("[TEST] error registering the \"grs\" command!");
    if(!_plugin_registercommand(pluginHandle, "graph", cbGraph, true))
        _plugin_logputs("[TEST] error registering the \"graph\" command!");
}

void testStop()
{
    _plugin_unregistercommand(pluginHandle, "plugin1");
    _plugin_unregistercommand(pluginHandle, "DumpProcess");
    _plugin_unregistercommand(pluginHandle, "grs");
    _plugin_menuclear(hMenu);
}

void testSetup()
{
    _plugin_menuaddentry(hMenu, MENU_DUMP, "&DumpProcess...");
    _plugin_menuaddentry(hMenu, MENU_TEST, "&Menu Test");
    _plugin_menuaddentry(hMenu, MENU_SELECTION, "&Selection API Test");
    _plugin_menuaddentry(hMenu, MENU_FILEOFFSET, "Follow &File Offset...");
}

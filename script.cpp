#include "script.h"
#include "angelscript\angelscript.h"
#include "angelscript\scriptstdstring.h"
#include "pluginsdk\_scriptapi_debug.h"
#include "pluginsdk\_scriptapi_memory.h"
#include "pluginsdk\_scriptapi_register.h"

//
// Script Engine stuff
//

static void MessageCallback(const asSMessageInfo* msg, void* param)
{
    const char* type = "ERR ";
    if(msg->type == asMSGTYPE_WARNING)
        type = "WARN";
    else if(msg->type == asMSGTYPE_INFORMATION)
        type = "INFO";

    _plugin_logprintf("[TEST] %s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

// Function implementation with native calling convention
static void PrintString(std::string & str)
{
    _plugin_logprintf(str.c_str());
}

// Function implementation with generic script interface
static void PrintString_Generic(asIScriptGeneric* gen)
{
    PrintString(*(std::string*)gen->GetArgAddress(0));
}

#ifdef _DEBUG
#define VERIFY(x) assert((x) >= 0)
#else
#define VERIFY(x) x
#endif

static int StringTypeId;

static void asParseVarArgs(asIScriptGeneric* Gen, int ArgIndex, char* Buffer, size_t Size)
{
    // Zero any data
    memset(Buffer, 0, Size);

    // Convert each parameter to the va_list form
    va_list va = (va_list)Buffer;

    for(int i = ArgIndex; i < Gen->GetArgCount(); i++)
    {
        PVOID addr = *(PVOID*)Gen->GetAddressOfArg(i);
        int type = Gen->GetArgTypeId(i);
        int size = Gen->GetEngine()->GetSizeOfPrimitiveType(type);

        switch(type)
        {
        case asTYPEID_VOID:
            break;

        case asTYPEID_BOOL:
        case asTYPEID_UINT8:
        case asTYPEID_INT8:
        case asTYPEID_UINT16:
        case asTYPEID_INT16:
        case asTYPEID_UINT32:
        case asTYPEID_INT32:
        case asTYPEID_UINT64:
        case asTYPEID_INT64:
        case asTYPEID_FLOAT:
        case asTYPEID_DOUBLE:
            memcpy(va, addr, size);
            va += size;
            break;

        default:
            if(type == StringTypeId)
                va_arg(va, const char*) = ((std::string*)addr)->c_str();
            else
                memcpy(va, addr, size);
            va += size;
            break;
        }
    }
}

static void asPrintf(asIScriptGeneric* Gen)
{
    // Format is the first parameter
    std::string* format = static_cast<std::string*>(Gen->GetArgObject(0));

    // Buffer to store arguments for vsnprintf_s
    char data[256];
    asParseVarArgs(Gen, 1, data, sizeof(data));

    char buf[1024];
    vsnprintf_s(buf, _TRUNCATE, format->c_str(), (va_list)&data);

    // Send the string to the log window
    _plugin_logprintf(buf);
}

static void ConfigureEngine(asIScriptEngine* engine)
{

    // Register the script string type
    // Look at the implementation for this function for more information
    // on how to register a custom string type, and other object types.
    RegisterStdString(engine);

    VERIFY(engine->RegisterTypedef("byte", "uint8"));
    VERIFY(engine->RegisterTypedef("word", "uint16"));
    VERIFY(engine->RegisterTypedef("dword", "uint"));
    VERIFY(engine->RegisterTypedef("qword", "uint64"));

#ifdef _WIN64
    VERIFY(engine->RegisterTypedef("ptr", "uint64"));
    VERIFY(engine->RegisterTypedef("duint", "uint64"));
    VERIFY(engine->RegisterTypedef("handle", "uint64"));
#else
    VERIFY(engine->RegisterTypedef("ptr", "uint"));
    VERIFY(engine->RegisterTypedef("duint", "uint"));
    VERIFY(engine->RegisterTypedef("handle", "uint"));
#endif

    StringTypeId = engine->GetTypeIdByDecl("string");

    engine->SetDefaultNamespace("");

    VERIFY(engine->RegisterGlobalFunction("void Print(string &in format, ?&in = null, ?&in = null, ?&in = null, ?&in = null, ?&in = null, ?&in = null)", asFUNCTION(asPrintf), asCALL_GENERIC));

    VERIFY(engine->SetDefaultNamespace("Debug"));
    VERIFY(engine->RegisterGlobalFunction("void Wait()", asFUNCTION(Script::Debug::Wait), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("void Run()", asFUNCTION(Script::Debug::Run), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("void Pause()", asFUNCTION(Script::Debug::Pause), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("void Stop()", asFUNCTION(Script::Debug::Stop), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("void StepIn()", asFUNCTION(Script::Debug::StepIn), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("void StepOver()", asFUNCTION(Script::Debug::StepOver), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("void StepOut()", asFUNCTION(Script::Debug::StepOut), asCALL_CDECL));

    VERIFY(engine->SetDefaultNamespace("Memory"));
    VERIFY(engine->RegisterGlobalFunction("byte ReadByte(duint addr)", asFUNCTION(Script::Memory::ReadByte), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool WriteByte(duint addr, byte value)", asFUNCTION(Script::Memory::WriteByte), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word ReadWord(duint addr)", asFUNCTION(Script::Memory::ReadWord), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool WriteWord(duint addr, word value)", asFUNCTION(Script::Memory::WriteWord), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword ReadDword(duint addr)", asFUNCTION(Script::Memory::ReadDword), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool WriteDword(duint addr, dword value)", asFUNCTION(Script::Memory::WriteDword), asCALL_CDECL));
#ifdef _WIN64
    VERIFY(engine->RegisterGlobalFunction("qword ReadQword(duint addr)", asFUNCTION(Script::Memory::ReadQword), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool WriteQword(duint addr, qword value)", asFUNCTION(Script::Memory::WriteQword), asCALL_CDECL));
#endif //_WIN64
    VERIFY(engine->RegisterGlobalFunction("duint ReadPtr(duint addr)", asFUNCTION(Script::Memory::ReadPtr), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool WritePtr(duint addr, duint value)", asFUNCTION(Script::Memory::WritePtr), asCALL_CDECL));

    VERIFY(engine->SetDefaultNamespace("Register"));
    VERIFY(engine->RegisterGlobalFunction("duint GetDR0()", asFUNCTION(Script::Register::GetDR0), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetDR0(duint value)", asFUNCTION(Script::Register::SetDR0), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("duint GetDR1()", asFUNCTION(Script::Register::GetDR1), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetDR1(duint value)", asFUNCTION(Script::Register::SetDR1), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("duint GetDR2()", asFUNCTION(Script::Register::GetDR2), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetDR2(duint value)", asFUNCTION(Script::Register::SetDR2), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("duint GetDR3()", asFUNCTION(Script::Register::GetDR3), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetDR3(duint value)", asFUNCTION(Script::Register::SetDR3), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("duint GetDR6()", asFUNCTION(Script::Register::GetDR6), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetDR6(duint value)", asFUNCTION(Script::Register::SetDR6), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("duint GetDR7()", asFUNCTION(Script::Register::GetDR7), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetDR7(duint value)", asFUNCTION(Script::Register::SetDR7), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetEAX()", asFUNCTION(Script::Register::GetEAX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetEAX(dword value)", asFUNCTION(Script::Register::SetEAX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetAX()", asFUNCTION(Script::Register::GetAX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetAX(word value)", asFUNCTION(Script::Register::SetAX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetAH()", asFUNCTION(Script::Register::GetAH), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetAH(byte value)", asFUNCTION(Script::Register::SetAH), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetAL()", asFUNCTION(Script::Register::GetAL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetAL(byte value)", asFUNCTION(Script::Register::SetAL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetEBX()", asFUNCTION(Script::Register::GetEBX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetEBX(dword value)", asFUNCTION(Script::Register::SetEBX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetBX()", asFUNCTION(Script::Register::GetBX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetBX(word value)", asFUNCTION(Script::Register::SetBX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetBH()", asFUNCTION(Script::Register::GetBH), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetBH(byte value)", asFUNCTION(Script::Register::SetBH), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetBL()", asFUNCTION(Script::Register::GetBL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetBL(byte value)", asFUNCTION(Script::Register::SetBL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetECX()", asFUNCTION(Script::Register::GetECX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetECX(dword value)", asFUNCTION(Script::Register::SetECX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetCX()", asFUNCTION(Script::Register::GetCX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetCX(word value)", asFUNCTION(Script::Register::SetCX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetCH()", asFUNCTION(Script::Register::GetCH), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetCH(byte value)", asFUNCTION(Script::Register::SetCH), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetCL()", asFUNCTION(Script::Register::GetCL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetCL(byte value)", asFUNCTION(Script::Register::SetCL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetEDX()", asFUNCTION(Script::Register::GetEDX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetEDX(dword value)", asFUNCTION(Script::Register::SetEDX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetDX()", asFUNCTION(Script::Register::GetDX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetDX(word value)", asFUNCTION(Script::Register::SetDX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetDH()", asFUNCTION(Script::Register::GetDH), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetDH(byte value)", asFUNCTION(Script::Register::SetDH), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetDL()", asFUNCTION(Script::Register::GetDL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetDL(byte value)", asFUNCTION(Script::Register::SetDL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetEDI()", asFUNCTION(Script::Register::GetEDI), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetEDI(dword value)", asFUNCTION(Script::Register::SetEDI), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetDI()", asFUNCTION(Script::Register::GetDI), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetDI(word value)", asFUNCTION(Script::Register::SetDI), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetESI()", asFUNCTION(Script::Register::GetESI), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetESI(dword value)", asFUNCTION(Script::Register::SetESI), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetSI()", asFUNCTION(Script::Register::GetSI), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetSI(word value)", asFUNCTION(Script::Register::SetSI), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetEBP()", asFUNCTION(Script::Register::GetEBP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetEBP(dword value)", asFUNCTION(Script::Register::SetEBP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetBP()", asFUNCTION(Script::Register::GetBP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetBP(word value)", asFUNCTION(Script::Register::SetBP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetESP()", asFUNCTION(Script::Register::GetESP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetESP(dword value)", asFUNCTION(Script::Register::SetESP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetSP()", asFUNCTION(Script::Register::GetSP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetSP(word value)", asFUNCTION(Script::Register::SetSP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetEIP()", asFUNCTION(Script::Register::GetEIP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetEIP(dword value)", asFUNCTION(Script::Register::SetEIP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("duint GetCIP()", asFUNCTION(Script::Register::GetCIP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetCIP(duint value)", asFUNCTION(Script::Register::SetCIP), asCALL_CDECL));
#ifdef _WIN64
    VERIFY(engine->RegisterGlobalFunction("uint64 GetRAX()", asFUNCTION(Script::Register::GetRAX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetRAX(uint64 value)", asFUNCTION(Script::Register::SetRAX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetRBX()", asFUNCTION(Script::Register::GetRBX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetRBX(uint64 value)", asFUNCTION(Script::Register::SetRBX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetRCX()", asFUNCTION(Script::Register::GetRCX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetRCX(uint64 value)", asFUNCTION(Script::Register::SetRCX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetRDX()", asFUNCTION(Script::Register::GetRDX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetRDX(uint64 value)", asFUNCTION(Script::Register::SetRDX), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetRSI()", asFUNCTION(Script::Register::GetRSI), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetRSI(uint64 value)", asFUNCTION(Script::Register::SetRSI), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetSIL()", asFUNCTION(Script::Register::GetSIL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetSIL(byte value)", asFUNCTION(Script::Register::SetSIL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetRDI()", asFUNCTION(Script::Register::GetRDI), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetRDI(uint64 value)", asFUNCTION(Script::Register::SetRDI), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetDIL()", asFUNCTION(Script::Register::GetDIL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetDIL(byte value)", asFUNCTION(Script::Register::SetDIL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetRBP()", asFUNCTION(Script::Register::GetRBP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetRBP(uint64 value)", asFUNCTION(Script::Register::SetRBP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetBPL()", asFUNCTION(Script::Register::GetBPL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetBPL(byte value)", asFUNCTION(Script::Register::SetBPL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetRSP()", asFUNCTION(Script::Register::GetRSP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetRSP(uint64 value)", asFUNCTION(Script::Register::SetRSP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetSPL()", asFUNCTION(Script::Register::GetSPL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetSPL(byte value)", asFUNCTION(Script::Register::SetSPL), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetRIP()", asFUNCTION(Script::Register::GetRIP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetRIP(uint64 value)", asFUNCTION(Script::Register::SetRIP), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetR8()", asFUNCTION(Script::Register::GetR8), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR8(uint64 value)", asFUNCTION(Script::Register::SetR8), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetR8D()", asFUNCTION(Script::Register::GetR8D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR8D(dword value)", asFUNCTION(Script::Register::SetR8D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetR8W()", asFUNCTION(Script::Register::GetR8W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR8W(word value)", asFUNCTION(Script::Register::SetR8W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetR8B()", asFUNCTION(Script::Register::GetR8B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR8B(byte value)", asFUNCTION(Script::Register::SetR8B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetR9()", asFUNCTION(Script::Register::GetR9), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR9(uint64 value)", asFUNCTION(Script::Register::SetR9), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetR9D()", asFUNCTION(Script::Register::GetR9D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR9D(dword value)", asFUNCTION(Script::Register::SetR9D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetR9W()", asFUNCTION(Script::Register::GetR9W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR9W(word value)", asFUNCTION(Script::Register::SetR9W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetR9B()", asFUNCTION(Script::Register::GetR9B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR9B(byte value)", asFUNCTION(Script::Register::SetR9B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetR10()", asFUNCTION(Script::Register::GetR10), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR10(uint64 value)", asFUNCTION(Script::Register::SetR10), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetR10D()", asFUNCTION(Script::Register::GetR10D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR10D(dword value)", asFUNCTION(Script::Register::SetR10D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetR10W()", asFUNCTION(Script::Register::GetR10W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR10W(word value)", asFUNCTION(Script::Register::SetR10W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetR10B()", asFUNCTION(Script::Register::GetR10B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR10B(byte value)", asFUNCTION(Script::Register::SetR10B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetR11()", asFUNCTION(Script::Register::GetR11), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR11(uint64 value)", asFUNCTION(Script::Register::SetR11), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetR11D()", asFUNCTION(Script::Register::GetR11D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR11D(dword value)", asFUNCTION(Script::Register::SetR11D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetR11W()", asFUNCTION(Script::Register::GetR11W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR11W(word value)", asFUNCTION(Script::Register::SetR11W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetR11B()", asFUNCTION(Script::Register::GetR11B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR11B(byte value)", asFUNCTION(Script::Register::SetR11B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetR12()", asFUNCTION(Script::Register::GetR12), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR12(uint64 value)", asFUNCTION(Script::Register::SetR12), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetR12D()", asFUNCTION(Script::Register::GetR12D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR12D(dword value)", asFUNCTION(Script::Register::SetR12D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetR12W()", asFUNCTION(Script::Register::GetR12W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR12W(word value)", asFUNCTION(Script::Register::SetR12W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetR12B()", asFUNCTION(Script::Register::GetR12B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR12B(byte value)", asFUNCTION(Script::Register::SetR12B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetR13()", asFUNCTION(Script::Register::GetR13), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR13(uint64 value)", asFUNCTION(Script::Register::SetR13), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetR13D()", asFUNCTION(Script::Register::GetR13D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR13D(dword value)", asFUNCTION(Script::Register::SetR13D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetR13W()", asFUNCTION(Script::Register::GetR13W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR13W(word value)", asFUNCTION(Script::Register::SetR13W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetR13B()", asFUNCTION(Script::Register::GetR13B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR13B(byte value)", asFUNCTION(Script::Register::SetR13B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetR14()", asFUNCTION(Script::Register::GetR14), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR14(uint64 value)", asFUNCTION(Script::Register::SetR14), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetR14D()", asFUNCTION(Script::Register::GetR14D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR14D(dword value)", asFUNCTION(Script::Register::SetR14D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetR14W()", asFUNCTION(Script::Register::GetR14W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR14W(word value)", asFUNCTION(Script::Register::SetR14W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetR14B()", asFUNCTION(Script::Register::GetR14B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR14B(byte value)", asFUNCTION(Script::Register::SetR14B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("uint64 GetR15()", asFUNCTION(Script::Register::GetR15), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR15(uint64 value)", asFUNCTION(Script::Register::SetR15), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("dword GetR15D()", asFUNCTION(Script::Register::GetR15D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR15D(dword value)", asFUNCTION(Script::Register::SetR15D), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("word GetR15W()", asFUNCTION(Script::Register::GetR15W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR15W(word value)", asFUNCTION(Script::Register::SetR15W), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("byte GetR15B()", asFUNCTION(Script::Register::GetR15B), asCALL_CDECL));
    VERIFY(engine->RegisterGlobalFunction("bool SetR15B(byte value)", asFUNCTION(Script::Register::SetR15B), asCALL_CDECL));
#endif //_WIN64
}

static int CompileScript(asIScriptEngine* engine)
{
    int r;

    char scriptFile[GUI_MAX_LINE_SIZE] = "";
    if(!GuiGetLineWindow("Script file", scriptFile))
    {
        _plugin_logputs("[TEST] No script to open...");
        return -1;
    }

    // We will load the script from a file on the disk.
    FILE* f = fopen(scriptFile, "rb");
    if(f == 0)
    {
        _plugin_logprintf("[TEST] Failed to open the script file \"%s\"\n", scriptFile);
        return -1;
    }

    // Determine the size of the file
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    fseek(f, 0, SEEK_SET);

    // On Win32 it is possible to do the following instead
    // int len = _filelength(_fileno(f));

    // Read the entire file
    std::string script;
    script.resize(len);
    size_t c = fread(&script[0], len, 1, f);
    fclose(f);

    if(c == 0)
    {
        _plugin_logprintf("[TEST] Failed to load the script file \"%s\"\n", scriptFile);
        return -1;
    }

    // Add the script sections that will be compiled into executable code.
    // If we want to combine more than one file into the same script, then
    // we can call AddScriptSection() several times for the same module and
    // the script engine will treat them all as if they were one. The script
    // section name, will allow us to localize any errors in the script code.
    asIScriptModule* mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
    r = mod->AddScriptSection("script", &script[0], len);
    if(r < 0)
    {
        _plugin_logputs("[TEST] AddScriptSection() failed!");
        return -1;
    }

    // Compile the script. If there are any compiler messages they will
    // be written to the message stream that we set right after creating the
    // script engine. If there are no errors, and no warnings, nothing will
    // be written to the stream.
    r = mod->Build();
    if(r < 0)
    {
        _plugin_logputs("[TEST] Build() failed!");
        return -1;
    }

    // The engine doesn't keep a copy of the script sections after Build() has
    // returned. So if the script needs to be recompiled, then all the script
    // sections must be added again.

    // If we want to have several scripts executing at different times but
    // that have no direct relation with each other, then we can compile them
    // into separate script modules. Each module use their own namespace and
    // scope, so function names, and global variables will not conflict with
    // each other.

    return 0;
}

static int RunApplication()
{
    int r;

    // Create the script engine
    asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    if(engine == 0)
    {
        _plugin_logputs("[TEST] Failed to create script engine!");
        return -1;
    }

    // The script compiler will write any compiler messages to the callback.
    engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);

    // Configure the script engine with all the functions,
    // and variables that the script should be able to use.
    ConfigureEngine(engine);

    // Compile the script code
    r = CompileScript(engine);
    if(r < 0)
    {
        engine->Release();
        return -1;
    }

    // Create a context that will execute the script.
    asIScriptContext* ctx = engine->CreateContext();
    if(ctx == 0)
    {
        _plugin_logputs("[TEST] Failed to create the context");
        engine->Release();
        return -1;
    }

    // Find the function for the function we want to execute.
    asIScriptFunction* func = engine->GetModule(0)->GetFunctionByDecl("void main()");
    if(func == 0)
    {
        _plugin_logputs("[TEST] The function 'void main()' was not found!");
        ctx->Release();
        engine->Release();
        return -1;
    }

    // Prepare the script context with the function we wish to execute. Prepare()
    // must be called on the context before each new script function that will be
    // executed. Note, that if you intend to execute the same function several
    // times, it might be a good idea to store the function returned by
    // GetFunctionByDecl(), so that this relatively slow call can be skipped.
    r = ctx->Prepare(func);
    if(r < 0)
    {
        _plugin_logputs("[TEST] Failed to prepare the context!");
        ctx->Release();
        engine->Release();
        return -1;
    }

    // Execute the function
    _plugin_logputs("[TEST] Executing the script...");
    r = ctx->Execute();
    if(r != asEXECUTION_FINISHED)
    {
        // The execution didn't finish as we had planned. Determine why.
        if(r == asEXECUTION_ABORTED)
            _plugin_logputs("[TEST] The script was aborted before it could finish.");
        else if(r == asEXECUTION_EXCEPTION)
        {
            _plugin_logputs("[TEST] The script ended with an exception.");

            // Write some information about the script exception
            asIScriptFunction* func = ctx->GetExceptionFunction();
            _plugin_logprintf("[TEST] func: %s\n", func->GetDeclaration());
            _plugin_logprintf("[TEST] modl: %s\n", func->GetModuleName());
            _plugin_logprintf("[TEST] sect: %s\n", func->GetScriptSectionName());
            _plugin_logprintf("[TEST] line: %d\n", ctx->GetExceptionLineNumber());
            _plugin_logprintf("[TEST] desc: %s\n", ctx->GetExceptionString());
        }
        else
            _plugin_logprintf("[TEST] The script ended for some unforeseen reason %d\n", r);
    }
    else
    {
        _plugin_logputs("[TEST] The script function returned.");
    }

    // We must release the contexts when no longer using them
    ctx->Release();

    // Shut down the engine
    engine->ShutDownAndRelease();

    return 0;
}

void cbScript()
{
    RunApplication();
}
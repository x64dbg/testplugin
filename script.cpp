#include "script.h"
#include "angelscript\angelscript.h"
#include "angelscript\scriptstdstring.h"

//
// Script bindings
//

static void wait()
{
    _plugin_waituntilpaused();
}

static void stepOver()
{
    DbgCmdExecDirect("StepOver");
    wait();
}

static void stepIn()
{
    DbgCmdExecDirect("StepInto");
    wait();
}

static void stepOut()
{
    DbgCmdExecDirect("StepOut");
    wait();
}

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

static void ConfigureEngine(asIScriptEngine* engine)
{
    int r;

    // Register the script string type
    // Look at the implementation for this function for more information
    // on how to register a custom string type, and other object types.
    RegisterStdString(engine);

    if(!strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY"))
    {
        // Register the functions that the scripts will be allowed to use.
        // Note how the return code is validated with an assert(). This helps
        // us discover where a problem occurs, and doesn't pollute the code
        // with a lot of if's. If an error occurs in release mode it will
        // be caught when a script is being built, so it is not necessary
        // to do the verification here as well.
        r = engine->RegisterGlobalFunction("void Print(string &in)", asFUNCTION(PrintString), asCALL_CDECL);
        r = engine->RegisterGlobalFunction("void StepIn()", asFUNCTION(stepIn), asCALL_CDECL);
        r = engine->RegisterGlobalFunction("void StepOver()", asFUNCTION(stepOver), asCALL_CDECL);
        r = engine->RegisterGlobalFunction("void StepOut()", asFUNCTION(stepOut), asCALL_CDECL);
    }
    else
    {
        // Notice how the registration is almost identical to the above.
        r = engine->RegisterGlobalFunction("void Print(string &in)", asFUNCTION(PrintString_Generic), asCALL_GENERIC);
        r = engine->RegisterGlobalFunction("void StepIn()", asFUNCTION(stepIn), asCALL_GENERIC);
        r = engine->RegisterGlobalFunction("void StepOver()", asFUNCTION(stepOver), asCALL_GENERIC);
        r = engine->RegisterGlobalFunction("void StepOut()", asFUNCTION(stepOut), asCALL_GENERIC);
    }


    // It is possible to register the functions, properties, and types in
    // configuration groups as well. When compiling the scripts it then
    // be defined which configuration groups should be available for that
    // script. If necessary a configuration group can also be removed from
    // the engine, so that the engine configuration could be changed
    // without having to recompile all the scripts.
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
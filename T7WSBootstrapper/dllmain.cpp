// dllmain.cpp : Defines the entry point for the DLL application.
#include "..\framework.h"
#include <filesystem>

extern "C" __declspec(dllexport) void dummy_proc() { }

void check_update_moves()
{
    if (std::filesystem::exists(WSTORE_UPDATER_DEST_FILENAME))
    {
        if (std::filesystem::copy_file(WSTORE_UPDATER_DEST_FILENAME, WSINTERNAL_MOD_NAME, std::filesystem::copy_options::overwrite_existing))
        {
            std::filesystem::remove(WSTORE_UPDATER_DEST_FILENAME);
        }
    }
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        check_update_moves();
        LoadLibraryA(WSINTERNAL_MOD_NAME);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


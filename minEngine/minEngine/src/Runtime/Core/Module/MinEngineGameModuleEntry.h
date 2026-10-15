#pragma once

/**
 * Every game SHARED linked via minengine_add_*_executable(GAME_MODULE)
 * must export this C symbol. Hosts force an unresolved ref (-u /INCLUDE) so MinGW
 * puts the DLL in the PE import table and ME_CLASS static registration runs.
 *
 * Usage (one TU in the game module, with your export macro):
 *   #include "Runtime/Core/Module/MinEngineGameModuleEntry.h"
 *   MINENGINE_IMPLEMENT_GAME_MODULE_ENTRY(MYGAME_API)
 */
#define MINENGINE_GAME_MODULE_ENTRY_NAME MinEngine_GameModuleEntry

#define MINENGINE_IMPLEMENT_GAME_MODULE_ENTRY(API_MACRO) \
    extern "C" API_MACRO int MinEngine_GameModuleEntry(void) \
    { \
        return 1; \
    }

// {{PROJECT_NAME}} game module entry (BUILD-F01 S07 force-load contract).

#include "Runtime/Core/Module/MinEngineGameModuleEntry.h"

#if defined(_WIN32) && defined({{GAME_MODULE_NAME}}_BUILD)
#    define {{GAME_MODULE_NAME}}_API __declspec(dllexport)
#elif defined(_WIN32)
#    define {{GAME_MODULE_NAME}}_API __declspec(dllimport)
#else
#    define {{GAME_MODULE_NAME}}_API __attribute__((visibility("default")))
#endif

MINENGINE_IMPLEMENT_GAME_MODULE_ENTRY({{GAME_MODULE_NAME}}_API)

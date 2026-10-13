// {{PROJECT_NAME}} game module entry (generated template).

#if defined(_WIN32) && defined({{GAME_MODULE_NAME}}_BUILD)
#    define {{GAME_MODULE_NAME}}_API __declspec(dllexport)
#elif defined(_WIN32)
#    define {{GAME_MODULE_NAME}}_API __declspec(dllimport)
#else
#    define {{GAME_MODULE_NAME}}_API __attribute__((visibility("default")))
#endif

extern "C" {{GAME_MODULE_NAME}}_API int {{GAME_MODULE_NAME}}_ModuleMarker(void)
{
    return 1;
}

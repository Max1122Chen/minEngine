// Minimal SHARED marker for ED-F20 Dev-host link smoke.
// Export a callable symbol so linkers keep the load-time dependency.

#if defined(_WIN32) && defined(GAMEMODULESTUB_BUILD)
#    define GAMEMODULESTUB_API __declspec(dllexport)
#elif defined(_WIN32)
#    define GAMEMODULESTUB_API __declspec(dllimport)
#else
#    define GAMEMODULESTUB_API __attribute__((visibility("default")))
#endif

extern "C" GAMEMODULESTUB_API int MinEngine_GameModuleStub_Marker(void)
{
    return 1;
}

// ED-F20: optional load-time reference to the linked game SHARED.
// Stub builds define MINENGINE_LINK_GAME_MODULE_STUB and export a marker.
// Other games do not need this TU to call into them (link + COPY_RUNTIME is enough).

#if defined(MINENGINE_HAS_GAME_MODULE) && MINENGINE_HAS_GAME_MODULE

#if defined(MINENGINE_LINK_GAME_MODULE_STUB) && MINENGINE_LINK_GAME_MODULE_STUB
extern "C" int MinEngine_GameModuleStub_Marker(void);

namespace
{
    struct GameModuleStubKeepAlive
    {
        GameModuleStubKeepAlive()
        {
            (void)MinEngine_GameModuleStub_Marker();
        }
    };

    const GameModuleStubKeepAlive g_GameModuleStubKeepAlive;
}
#endif

#endif

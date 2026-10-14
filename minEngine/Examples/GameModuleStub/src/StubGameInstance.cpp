#include "StubGameInstance.h"

#include "StubGameInstanceSubsystem.h"

namespace GameModuleStub
{
    int StubGameInstance::s_OnInitCount = 0;
    int StubGameInstance::s_OnShutdownCount = 0;
    bool StubGameInstance::s_SawGiSubsystemOnInit = false;

    void StubGameInstance::OnInit()
    {
        ++s_OnInitCount;
        s_SawGiSubsystemOnInit = GetSubsystem<StubGameInstanceSubsystem>() != nullptr;
    }

    void StubGameInstance::OnShutdown()
    {
        ++s_OnShutdownCount;
    }
}

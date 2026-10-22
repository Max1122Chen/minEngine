#pragma once

#include "GameModuleStubAPI.h"
#include "Runtime/Function/Framework/GameInstance/GameInstance.h"

namespace GameModuleStub
{
    /**
     * Sample session hub. Set ProjectSettings.GameInstanceClass to
     * "GameModuleStub::StubGameInstance", or Engine::SetGameInstanceClassOverride in tests.
     */
    ME_CLASS()
    class StubGameInstance : public minEngine::GameInstance
    {
        ME_GENERATED_BODY()

    public:
        static int s_OnInitCount;
        static int s_OnShutdownCount;
        static bool s_SawGiSubsystemOnInit;

        void OnInit() override;
        void OnShutdown() override;
    };
}

#include "StubGameInstance.gen.h"

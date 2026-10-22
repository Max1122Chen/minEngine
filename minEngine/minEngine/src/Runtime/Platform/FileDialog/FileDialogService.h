#pragma once

#include "Runtime/Core/EngineAPI.h"

#include "IFileDialogService.h"

#include <memory>

namespace minEngine
{
    class MINENGINE_API FileDialogService
    {
    public:
        static FileDialogService& Get();
        static bool HasInstance();

        void Initialize();
        void Shutdown();

        bool IsReady() const { return m_NfdReady; }

        IFileDialogService& GetImplementation();
        const IFileDialogService& GetImplementation() const;

    private:
        friend class Engine;

        static void SetInstance(FileDialogService* instance);

        std::unique_ptr<IFileDialogService> m_Implementation;
        bool m_NfdReady = false;
        static FileDialogService* s_Instance;
    };
}

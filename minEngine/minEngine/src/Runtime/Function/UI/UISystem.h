#pragma once

#include "Runtime/Function/UI/ScreenUIHitTester.h"
#include "Runtime/Function/UI/ScreenUIHitTypes.h"

namespace minEngine
{
    class Scene;

    class UISystem
    {
    public:
        UISystem();
        ~UISystem();

        UISystem(const UISystem&) = delete;
        UISystem& operator=(const UISystem&) = delete;

        void Initialize();
        void Shutdown();

        static bool HasInstance();
        static UISystem& Get();

        void Tick(float deltaTime);

        void SetPointerViewportContext(const ScreenUIPointerViewportContext& context);
        const ScreenUIPointerViewportContext& GetPointerViewportContext() const { return m_ViewportContext; }

        const ScreenUIPointerState& GetPointerState() const { return m_PointerState; }

        ScreenUIHitResult HitTestAt(const ScreenUIHitQuery& query) const;
        ScreenUIHitResult HitTestAtWindowMouse(Scene* scene) const;

        void SetPointerRoutingEnabled(bool enabled);
        bool IsPointerRoutingEnabled() const { return m_bPointerRoutingEnabled; }

        /**
         * True when ScreenUI should consume the pointer for world/editor pick.
         * Call from pick / gameplay consumers (S2). Requires routing enabled.
         */
        bool ShouldBlockWorldPointer() const;

        /**
         * Re-sample pointer against the current viewport context.
         * Editor injects ImageMin/Size then calls this so Hover matches the drawn frame.
         */
        void PollPointer();

        void OnBeginPIE(Scene* pieScene);
        void OnEndPIE(Scene* pieScene);

    private:
        friend class Engine;
        friend class ScreenUIHitTestScope;

        static void SetInstance(UISystem* instance);

        void UpdatePointerFromInput(Scene* scene);
        Scene* ResolvePointerScene() const;

        static UISystem* s_Instance;

        bool m_Initialized = false;
        bool m_bPointerRoutingEnabled = false;

        ScreenUIHitTester m_HitTester;
        ScreenUIPointerState m_PointerState;
        ScreenUIPointerViewportContext m_ViewportContext;
        Scene* m_PIEScene = nullptr;
    };
}

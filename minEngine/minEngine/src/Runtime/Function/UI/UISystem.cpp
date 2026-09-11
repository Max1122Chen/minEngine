#include "Runtime/Function/UI/UISystem.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Components/ButtonComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Input/InputKeys.h"
#include "Runtime/Function/Input/InputSystem.h"

namespace minEngine
{
    UISystem* UISystem::s_Instance = nullptr;

    UISystem::UISystem() = default;

    UISystem::~UISystem()
    {
        Shutdown();
    }

    void UISystem::SetInstance(UISystem* instance)
    {
        s_Instance = instance;
    }

    UISystem& UISystem::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "UISystem is not initialized");
        return *s_Instance;
    }

    bool UISystem::HasInstance()
    {
        return s_Instance != nullptr;
    }

    void UISystem::Initialize()
    {
        if (m_Initialized)
        {
            return;
        }

        m_PointerState.Reset();
        m_ViewportContext.Reset();
        m_bPointerRoutingEnabled = false;
        m_PIEScene = nullptr;
        m_Initialized = true;
        ME_LOG(LogUI, Info, "UISystem Initialized");
    }

    void UISystem::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        m_PointerState.Reset();
        m_ViewportContext.Reset();
        m_bPointerRoutingEnabled = false;
        m_PIEScene = nullptr;
        m_Initialized = false;
        ME_LOG(LogUI, Info, "UISystem Shutdown");
    }

    void UISystem::Tick(float deltaTime)
    {
        (void)deltaTime;
        if (!m_Initialized)
        {
            return;
        }

        m_PointerState.bClickThisFrame = false;
        m_PointerState.Clicked = nullptr;
        m_PointerState.bButtonOnClickedThisFrame = false;

        if (!m_bPointerRoutingEnabled)
        {
            return;
        }

        UpdatePointerFromInput(ResolvePointerScene());
    }

    void UISystem::SetPointerViewportContext(const ScreenUIPointerViewportContext& context)
    {
        m_ViewportContext = context;
    }

    bool UISystem::ShouldBlockWorldPointer() const
    {
        if (!m_Initialized || !m_bPointerRoutingEnabled)
        {
            return false;
        }

        return m_PointerState.Hovered != nullptr || m_PointerState.Pressed != nullptr;
    }

    void UISystem::PollPointer()
    {
        if (!m_Initialized || !m_bPointerRoutingEnabled)
        {
            return;
        }

        UpdatePointerFromInput(ResolvePointerScene());
    }

    Scene* UISystem::ResolvePointerScene() const
    {
        if (m_PIEScene != nullptr)
        {
            return m_PIEScene;
        }

        if (SceneManager::HasInstance())
        {
            const std::shared_ptr<Scene> active = SceneManager::Get().GetCurrentActiveScene();
            return active.get();
        }

        return nullptr;
    }

    ScreenUIHitResult UISystem::HitTestAt(const ScreenUIHitQuery& query) const
    {
        return m_HitTester.HitTestAt(query);
    }

    ScreenUIHitResult UISystem::HitTestAtWindowMouse(Scene* scene) const
    {
        ScreenUIHitResult result;
        result.Reset();

        if (scene == nullptr || !m_ViewportContext.bValid)
        {
            return result;
        }

        if (m_ViewportContext.ImageSize.x <= 0.0f || m_ViewportContext.ImageSize.y <= 0.0f)
        {
            return result;
        }

        if (!InputSystem::HasInstance())
        {
            return result;
        }

        const Vector2 windowMouse = InputSystem::GetMousePosition();
        ScreenUIHitQuery query;
        query.ViewportPoint = windowMouse - m_ViewportContext.ImageMin;
        query.ViewportWidth = m_ViewportContext.ImageSize.x;
        query.ViewportHeight = m_ViewportContext.ImageSize.y;
        query.Scene = scene;
        return HitTestAt(query);
    }

    void UISystem::SetPointerRoutingEnabled(bool enabled)
    {
        m_bPointerRoutingEnabled = enabled;
        if (!enabled)
        {
            m_PointerState.Reset();
        }
    }

    void UISystem::OnBeginPIE(Scene* pieScene)
    {
        m_PIEScene = pieScene;
        SetPointerRoutingEnabled(true);
    }

    void UISystem::OnEndPIE(Scene* pieScene)
    {
        (void)pieScene;
        m_PIEScene = nullptr;
        m_ViewportContext.Reset();
        SetPointerRoutingEnabled(false);
    }

    void UISystem::TryDispatchButtonClick(WidgetComponent* pressedWidget)
    {
        if (pressedWidget == nullptr)
        {
            return;
        }

        GameObject* owner = pressedWidget->GetOwner();
        if (owner == nullptr)
        {
            return;
        }

        const std::vector<std::shared_ptr<ButtonComponent>> buttons =
            owner->GetComponentsOfType<ButtonComponent>();
        for (const std::shared_ptr<ButtonComponent>& button : buttons)
        {
            if (!button || !button->CanAcceptClick())
            {
                continue;
            }

            button->NotifyClicked();
            m_PointerState.bButtonOnClickedThisFrame = true;
        }
    }

    void UISystem::UpdatePointerFromInput(Scene* scene)
    {
        if (!InputSystem::HasInstance())
        {
            return;
        }

        const ScreenUIHitResult hit = HitTestAtWindowMouse(scene);
        m_PointerState.Hovered = hit.bHit ? hit.Widget : nullptr;

        if (InputSystem::MouseButtonPressed(InputKeys::Mouse_Left))
        {
            m_PointerState.bPointerDown = true;
            m_PointerState.Pressed = hit.bHit ? hit.Widget : nullptr;
        }

        if (InputSystem::MouseButtonReleased(InputKeys::Mouse_Left))
        {
            // Loose capture: Click belongs to Pressed even if pointer left the rect.
            // Dispatch before clearing Pressed so Button can resolve ownership.
            WidgetComponent* pressedWidget = m_PointerState.Pressed;
            if (pressedWidget != nullptr)
            {
                m_PointerState.bClickThisFrame = true;
                m_PointerState.Clicked = pressedWidget;
                TryDispatchButtonClick(pressedWidget);
            }
            m_PointerState.Pressed = nullptr;
            m_PointerState.bPointerDown = false;
        }
        else if (InputSystem::MouseButtonDown(InputKeys::Mouse_Left))
        {
            m_PointerState.bPointerDown = true;
        }
        else
        {
            m_PointerState.bPointerDown = false;
        }
    }
}

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "IPanel.h"

namespace minEngine
{
    class FunctionPanel final : public IPanel
    {
    public:
        using DrawCallback = std::function<void(const PanelContext&)>;

        FunctionPanel(std::string id, std::string title, DrawCallback drawCallback)
            : m_Id(std::move(id))
            , m_Title(std::move(title))
            , m_DrawCallback(std::move(drawCallback))
        {
        }

        const std::string& GetId() const override
        {
            return m_Id;
        }

        const std::string& GetTitle() const override
        {
            return m_Title;
        }

        void OnDraw(const PanelContext& context) override
        {
            if (m_DrawCallback)
            {
                m_DrawCallback(context);
            }
        }

    private:
        std::string m_Id;
        std::string m_Title;
        DrawCallback m_DrawCallback;
    };

    class PanelManager
    {
    public:
        IPanel* RegisterPanel(std::unique_ptr<IPanel> panel, const PanelContext& context)
        {
            if (!panel)
            {
                return nullptr;
            }

            const std::string id = panel->GetId();
            auto existing = m_IndexById.find(id);
            if (existing != m_IndexById.end())
            {
                return m_Panels[existing->second].get();
            }

            panel->OnAttach(context);
            m_Panels.emplace_back(std::move(panel));
            const size_t newIndex = m_Panels.size() - 1;
            m_IndexById[id] = newIndex;
            return m_Panels[newIndex].get();
        }

        bool TogglePanel(const std::string& id)
        {
            IPanel* panel = FindPanel(id);
            if (!panel)
            {
                return false;
            }

            panel->SetOpen(!panel->IsOpen());
            return true;
        }

        IPanel* FindPanel(const std::string& id)
        {
            auto iter = m_IndexById.find(id);
            if (iter == m_IndexById.end())
            {
                return nullptr;
            }

            return m_Panels[iter->second].get();
        }

        const IPanel* FindPanel(const std::string& id) const
        {
            auto iter = m_IndexById.find(id);
            if (iter == m_IndexById.end())
            {
                return nullptr;
            }

            return m_Panels[iter->second].get();
        }

        void TickPanels(const PanelContext& context)
        {
            for (const auto& panel : m_Panels)
            {
                if (!panel->IsOpen())
                {
                    continue;
                }

                panel->OnTick(context);
            }
        }

        void DrawPanels(const PanelContext& context)
        {
            for (const auto& panel : m_Panels)
            {
                if (!panel->IsOpen())
                {
                    continue;
                }

                panel->OnDraw(context);
            }
        }

        void Shutdown(const PanelContext& context)
        {
            for (auto iter = m_Panels.rbegin(); iter != m_Panels.rend(); ++iter)
            {
                (*iter)->OnDetach(context);
            }
            m_Panels.clear();
            m_IndexById.clear();
        }

        const std::vector<std::unique_ptr<IPanel>>& GetPanels() const
        {
            return m_Panels;
        }

    private:
        std::vector<std::unique_ptr<IPanel>> m_Panels;
        std::unordered_map<std::string, size_t> m_IndexById;
    };
}

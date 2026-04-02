#pragma once

#include <string>

#include "PanelContext.h"

namespace minEngine
{
    class IPanel
    {
    public:
        virtual ~IPanel() = default;

        virtual const std::string& GetId() const = 0;
        virtual const std::string& GetTitle() const = 0;

        bool IsOpen() const
        {
            return m_IsOpen;
        }

        void SetOpen(bool isOpen)
        {
            m_IsOpen = isOpen;
        }

        virtual void OnAttach(const PanelContext& context)
        {
            (void)context;
        }

        virtual void OnDetach(const PanelContext& context)
        {
            (void)context;
        }

        virtual void OnTick(const PanelContext& context)
        {
            (void)context;
        }

        virtual void OnDraw(const PanelContext& context) = 0;

    private:
        bool m_IsOpen = true;
    };
}

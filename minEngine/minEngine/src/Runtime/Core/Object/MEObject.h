#pragma once
#include "Core.h"

namespace minEngine
{
    ME_CLASS()
    class MEObject
    {
    public:
        virtual ~MEObject() = default;

        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& inName) { m_Name = inName; }


    protected:
        std::string m_Name;
        MEObject* m_Outer = nullptr;
    };
}
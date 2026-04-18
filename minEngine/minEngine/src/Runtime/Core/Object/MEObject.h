#pragma once
#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Reflection/Reflection.h"

namespace minEngine
{
    ME_CLASS()
    class MEObject
    {
        ME_REFLECTION_FRIEND(MEObject)
    public:
        virtual ~MEObject() = default;

        const Reflection::MEClass* GetClass() const { return m_Class; }
        void SetClass(const Reflection::MEClass* inClass) { m_Class = inClass; }

        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& inName) { m_Name = inName; }

        const GUID& GetGuid() const { return m_Guid; }
        void SetGuid(const GUID& inGuid) { m_Guid = inGuid; }

        const MEObject* GetOuter() const { return m_Outer; }
        void SetOuter(MEObject* inOuter) { m_Outer = inOuter; }


    protected:
        const Reflection::MEClass* m_Class = nullptr;

        ME_PROPERTY()
        std::string m_Name;
        GUID m_Guid;
        MEObject* m_Outer = nullptr;
    };

    
}

#include "MEObject.gen.h"
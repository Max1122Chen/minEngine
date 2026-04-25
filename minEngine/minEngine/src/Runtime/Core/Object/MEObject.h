#pragma once
#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Reflection/Reflection.h"

namespace minEngine::Reflection
{
    class ReflectionSystem;
    class MEClass;
}

namespace minEngine::Serialization
{
    class Serializer;
}

namespace minEngine
{
    
    ME_CLASS()
    class MEObject
    {
        ME_REFLECTION_FRIEND(MEObject)
        // Friend declaration for engine core classes 
        friend class Reflection::ReflectionSystem;
        friend class ObjectManager;
        friend class Serialization::Serializer;
        friend class AssetManager;
        // Friend declaration for editor classes
        friend class Editor;
    public:
        virtual ~MEObject() = default;

        const Reflection::MEClass* GetClass() const { return m_Class; }
        const std::string& GetName() const { return m_Name; }

        const GUID& GetGuid() const { return m_Guid; }

        const MEObject* GetOuter() const { return m_Outer; }
        void SetOuter(MEObject* inOuter) { m_Outer = inOuter; }

        bool IsA(const Reflection::MEClass* classInfo) const
        {
            return (m_Class != nullptr) && m_Class->IsA(classInfo);
        }
        
    protected:
        void SetClass(const Reflection::MEClass* inClass) { m_Class = inClass; }
        void SetName(const std::string& inName) { m_Name = inName; }
        void SetGuid(const GUID& inGuid) { m_Guid = inGuid; }
    protected:
        const Reflection::MEClass* m_Class = nullptr;

        ME_PROPERTY(Invisible)
        std::string m_Name;
        ME_PROPERTY(Invisible)
        GUID m_Guid;
        MEObject* m_Outer = nullptr;
    };

    
}

#include "MEObject.gen.h"
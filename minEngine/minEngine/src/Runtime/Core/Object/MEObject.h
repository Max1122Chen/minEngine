#pragma once
#include "Core.h"
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

        const MEObject* GetOuter() const { return m_Outer; }
        void SetOuter(MEObject* inOuter) { m_Outer = inOuter; }


    protected:
        const Reflection::MEClass* m_Class = nullptr;

        ME_PROPERTY()
        std::string m_Name;
        MEObject* m_Outer = nullptr;
    };

    template<typename T>
    static std::shared_ptr<T> NewObject(const std::string& name = "", MEObject* outer = nullptr)
    {
        static_assert(std::is_base_of_v<MEObject, T>, "T must be derived from MEObject");
        std::shared_ptr<T> newObj = std::make_shared<T>();
        newObj->SetClass(Reflection::ReflectionSystem::Get().FindClass<T>());
        newObj->SetName(name);
        newObj->SetOuter(outer);
        return newObj;
    }
}

#include "MEObject.gen.h"
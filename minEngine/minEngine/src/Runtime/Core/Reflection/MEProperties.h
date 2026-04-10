#pragma once

#include <string>
#include <utility>

namespace minEngine::MEReflection
{
    class MEClass;

    template<typename T>
    struct FieldAccessor;

    using FieldConstAccessorFn = const void* (*)(const void*);
    using FieldMutableAccessorFn = void* (*)(void*);

    enum class MEPropertyCategory
    {
        Primitive,
        Object,
        ObjectPtr,
        Array
    };

    class MEProperty
    {
    public:
        explicit MEProperty(std::string inName)
            : name(std::move(inName))
        {
        }

        virtual ~MEProperty() = default;

        std::string name;
        FieldConstAccessorFn constAccessor = nullptr;
        FieldMutableAccessorFn mutableAccessor = nullptr;

        virtual MEPropertyCategory GetCategory() const = 0;
    };

    class MEPrimitiveProperty : public MEProperty
    {
    public:
        MEPrimitiveProperty(std::string inName, std::string inPrimitiveTypeName)
            : MEProperty(std::move(inName)),
              primitiveTypeName(std::move(inPrimitiveTypeName))
        {
        }

        MEPropertyCategory GetCategory() const override
        {
            return MEPropertyCategory::Primitive;
        }

    public:
        std::string primitiveTypeName;
    };

    class MEObjectProperty : public MEProperty
    {
    public:
        explicit MEObjectProperty(std::string inName)
            : MEProperty(std::move(inName))
        {
        }

        MEPropertyCategory GetCategory() const override
        {
            return MEPropertyCategory::Object;
        }

        void SetValueClass(MEClass* inClass)
        {
            valueClass = inClass;
        }

        MEClass* GetValueClass() const
        {
            return valueClass;
        }

    private:
        MEClass* valueClass = nullptr;
    };

    class MEObjectPtrProperty final : public MEObjectProperty
    {
    public:
        explicit MEObjectPtrProperty(std::string inName)
            : MEObjectProperty(std::move(inName))
        {
        }

        MEPropertyCategory GetCategory() const override
        {
            return MEPropertyCategory::ObjectPtr;
        }
    };

    class MEArrayProperty final : public MEProperty
    {
    public:
        MEArrayProperty(std::string inName, MEProperty* inInnerProperty)
            : MEProperty(std::move(inName)),
              innerProperty(inInnerProperty)
        {
        }

        MEPropertyCategory GetCategory() const override
        {
            return MEPropertyCategory::Array;
        }

        void SetInnerProperty(MEProperty* inInnerProperty)
        {
            innerProperty = inInnerProperty;
        }

        MEProperty* GetInnerProperty() const
        {
            return innerProperty;
        }

    private:
        MEProperty* innerProperty = nullptr;
    };

    class MEIntProperty final : public MEPrimitiveProperty
    {
    public:
        explicit MEIntProperty(std::string inName)
            : MEPrimitiveProperty(std::move(inName), "int")
        {
        }
    };

}
#pragma once

#include <cstddef>
#include <string>
#include <utility>

namespace minEngine::Reflection
{
    class MEClass;

    template<typename T>
    struct FieldAccessor;

    template<typename TPointee>
    struct PointingValueAccessor
    {
        
    }

    using FieldConstAccessorFn = const void* (*)(const void*);
    using FieldMutableAccessorFn = void* (*)(void*);
    using MEArrayGetSizeFn = size_t (*)(const void*);
    using MEArrayGetConstElementFn = const void* (*)(const void*, size_t);
    using MEArrayResizeFn = void (*)(void*, size_t);
    using MEArrayGetMutableElementFn = void* (*)(void*, size_t);

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

    // Inherits from MEObjectProperty so that ObjectPtrProperty can reuse the valueClass member to store the class of the pointed object
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

        void SetArrayAccessors(MEArrayGetSizeFn inGetSize,
                               MEArrayGetConstElementFn inGetConstElement,
                               MEArrayResizeFn inResize,
                               MEArrayGetMutableElementFn inGetMutableElement)
        {
            getSize = inGetSize;
            getConstElement = inGetConstElement;
            resize = inResize;
            getMutableElement = inGetMutableElement;
        }

        size_t GetSize(const void* arrayObject) const
        {
            if (arrayObject == nullptr || getSize == nullptr)
            {
                return 0;
            }
            return getSize(arrayObject);
        }

        const void* GetConstElement(const void* arrayObject, size_t index) const
        {
            if (arrayObject == nullptr || getConstElement == nullptr)
            {
                return nullptr;
            }
            return getConstElement(arrayObject, index);
        }

        void Resize(void* arrayObject, size_t newSize) const
        {
            if (arrayObject == nullptr || resize == nullptr)
            {
                return;
            }
            resize(arrayObject, newSize);
        }

        void* GetMutableElement(void* arrayObject, size_t index) const
        {
            if (arrayObject == nullptr || getMutableElement == nullptr)
            {
                return nullptr;
            }
            return getMutableElement(arrayObject, index);
        }

        MEArrayGetSizeFn GetSizeAccessor() const
        {
            return getSize;
        }

        MEArrayGetConstElementFn GetConstElementAccessor() const
        {
            return getConstElement;
        }

        MEArrayResizeFn GetResizeAccessor() const
        {
            return resize;
        }

        MEArrayGetMutableElementFn GetMutableElementAccessor() const
        {
            return getMutableElement;
        }

    private:
        MEProperty* innerProperty = nullptr;
        MEArrayGetSizeFn getSize = nullptr;
        MEArrayGetConstElementFn getConstElement = nullptr;
        MEArrayResizeFn resize = nullptr;
        MEArrayGetMutableElementFn getMutableElement = nullptr;
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
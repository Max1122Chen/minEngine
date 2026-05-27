#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

#include "EngineAPI.h"
#include "MEEnum.h"
#include "TypeTraits.h"

namespace minEngine::Reflection
{
    class MEClass;

    template<typename T>
    struct FieldAccessor;

    // Field accessor function types
    using FieldConstAccessorFn = const void* (*)(const void*);
    using FieldMutableAccessorFn = void* (*)(void*);

    // Data underlying a pointer accessor function type, used for both raw pointers and smart pointers
    using PointingDataConstAccessorFn = const void* (*)(const void*);
    using PointingDataMutableAccessorFn = void* (*)(void*);

    // Array accessor function types
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

    enum class MEObjectPtrCategory
    {
        Invalid,
        Raw,
        Shared
    };

    enum class PropertySpecifier : uint32_t
    {
        None = 0u,
        Transient = 1u << 0,
        EditAnywhere = 1u << 1,
        EditDefaultsOnly = 1u << 2,
        EditInstanceOnly = 1u << 3,
        VisibleAnywhere = 1u << 4,
        VisibleDefaultsOnly = 1u << 5,
        VisibleInstanceOnly = 1u << 6,
        Invisible = 1u << 7,
        Instanced = 1u << 8,
    };

    using PropertySpecifierMask = uint32_t;
    using PropertyMetadata = std::unordered_map<std::string, std::string>;

    class MINENGINE_API MEProperty
    {
    public:
        explicit MEProperty(std::string inName)
            : name(std::move(inName))
        {
        }

        virtual ~MEProperty() = default;

        const std::string& GetName() const { return name; }
        void SetName(const std::string& inName) { name = inName; }

        FieldConstAccessorFn GetConstAccessor() const { return constAccessor; }
        FieldMutableAccessorFn GetMutableAccessor() const { return mutableAccessor; }
        void SetAccessors(FieldConstAccessorFn inConstAccessor, FieldMutableAccessorFn inMutableAccessor)
        {
            constAccessor = inConstAccessor;
            mutableAccessor = inMutableAccessor;
        }

        void* GetMutable(void* ptr) const { return mutableAccessor == nullptr ? nullptr : mutableAccessor(ptr); }
        const void* GetConst(const void* ptr) const { return constAccessor == nullptr ? nullptr : constAccessor(ptr); }

        PropertySpecifierMask GetSpecifierMask() const { return specifierMask; }
        void SetAnnotations(PropertySpecifierMask inSpecifierMask, PropertyMetadata inMetadata)
        {
            specifierMask = inSpecifierMask;
            metadata = std::move(inMetadata);
        }

        bool HasSpecifier(PropertySpecifier specifier) const
        {
            return (specifierMask & static_cast<PropertySpecifierMask>(specifier)) != 0;
        }

        const PropertyMetadata& GetMetadata() const { return metadata; }
        size_t GetStorageSize() const { return storageSize; }
        void SetStorageSize(size_t inStorageSize) { storageSize = inStorageSize; }
        size_t GetStorageAlignment() const { return storageAlignment; }
        void SetStorageAlignment(size_t inStorageAlignment) { storageAlignment = inStorageAlignment; }

        const std::string* FindMetadata(const std::string& key) const
        {
            auto iter = metadata.find(key);
            if (iter == metadata.end())
            {
                return nullptr;
            }
            return &iter->second;
        }

        virtual MEPropertyCategory GetCategory() const = 0;

    private:
        std::string name;
        FieldConstAccessorFn constAccessor = nullptr;
        FieldMutableAccessorFn mutableAccessor = nullptr;
        PropertySpecifierMask specifierMask = static_cast<PropertySpecifierMask>(PropertySpecifier::None);
        PropertyMetadata metadata;
        size_t storageSize = 0;
        size_t storageAlignment = 1;
    };

    class MINENGINE_API MEPrimitiveProperty : public MEProperty
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

        bool IsEnum() const
        {
            return boundEnum != nullptr;
        }

        const MEEnum* GetEnum() const
        {
            return boundEnum;
        }

        size_t GetSize() const
        {
            return boundEnum != nullptr ? boundEnum->GetSize() : 0;
        }

    public:
        std::string primitiveTypeName;
        const MEEnum* boundEnum = nullptr;
    };

    class MINENGINE_API MEObjectProperty : public MEProperty
    {
    public:
        explicit MEObjectProperty(std::string inName)
            : MEProperty(std::move(inName))
        {}

        MEPropertyCategory GetCategory() const override
        {
            return MEPropertyCategory::Object;
        }

        void SetValueClass(MEClass* inClass) { valueClass = inClass; }

        MEClass* GetValueClass() const { return valueClass; }

    protected:
        MEClass* valueClass = nullptr;
    };

    // Inherits from MEObjectProperty so that ObjectPtrProperty can reuse the valueClass member to store the class of the pointed object
    class MINENGINE_API MEObjectPtrProperty final : public MEObjectProperty
    {
    public:
        explicit MEObjectPtrProperty(std::string inName)
            : MEObjectProperty(std::move(inName))
        {}

        MEPropertyCategory GetCategory() const override
        {
            return MEPropertyCategory::ObjectPtr;
        }

        void SetPointingDataAccessors(PointingDataConstAccessorFn inConstAccessor,
                                     PointingDataMutableAccessorFn inMutableAccessor)
        {
            pointingDataConstAccessor = inConstAccessor;
            pointingDataMutableAccessor = inMutableAccessor;
        }

        const void* GetConstPointingData(void* ptrToPtr) const
        {
            return pointingDataConstAccessor == nullptr ? nullptr : pointingDataConstAccessor(ptrToPtr);
        }

        void* GetMutablePointingData(void* ptrToPtr) const
        {
            return pointingDataMutableAccessor == nullptr ? nullptr : pointingDataMutableAccessor(ptrToPtr);
        }

        void SetPtrCategory(MEObjectPtrCategory inPtrCategory) { ptrCategory = inPtrCategory; }
        MEObjectPtrCategory GetPtrCategory() const { return ptrCategory; }

    private:
        PointingDataConstAccessorFn pointingDataConstAccessor = nullptr;
        PointingDataMutableAccessorFn pointingDataMutableAccessor = nullptr;

        MEObjectPtrCategory ptrCategory = MEObjectPtrCategory::Invalid;
    };

    class MINENGINE_API MEArrayProperty final : public MEProperty
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
}
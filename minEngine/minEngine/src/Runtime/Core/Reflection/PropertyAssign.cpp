#include "PropertyAssign.h"

#include "Runtime/Core/Object/MEObject.h"

#include <cstring>

namespace minEngine::Reflection
{
    void CopyPropertyStorage(const MEProperty& property, void* dst, const void* src)
    {
        if (dst == nullptr || src == nullptr)
        {
            return;
        }

        if (property.GetValueCopyAssignFn() != nullptr)
        {
            property.CopyAssignValue(dst, src);
            return;
        }

        const size_t size = property.GetStorageSize();
        if (size == 0)
        {
            return;
        }

        std::memcpy(dst, src, size);
    }

    bool AssignProperty(void* owner, const MEProperty& property, const void* valuePtr)
    {
        // Static storage is suitably aligned; avoids MinGW AVX vmovdqa to a misaligned stack temporary.
        static const PropertyAssignOptions kDefaultOptions{};
        return AssignProperty(owner, property, valuePtr, kDefaultOptions);
    }

    bool AssignProperty(void* owner,
                        const MEProperty& property,
                        const void* valuePtr,
                        const PropertyAssignOptions& options)
    {
        if (owner == nullptr || valuePtr == nullptr)
        {
            return false;
        }

        if (property.HasPropertySetter())
        {
            property.GetPropertySetter()(owner, valuePtr);
        }
        else
        {
            if (property.GetMutableAccessor() == nullptr)
            {
                return false;
            }

            void* dst = property.GetMutable(owner);
            if (dst == nullptr)
            {
                return false;
            }

            CopyPropertyStorage(property, dst, valuePtr);
        }

        if (options.notifyPostEdit)
        {
            MEObject* postEditObject = options.postEditObject;
            if (postEditObject == nullptr)
            {
                postEditObject = static_cast<MEObject*>(owner);
            }

            const std::string_view name =
                options.postEditPropertyName.empty() ? std::string_view(property.GetName())
                                                     : options.postEditPropertyName;
            postEditObject->PostEditChangeProperty(PropertyChangedEvent{name});
        }

        return true;
    }

    bool GetPropertyValue(const void* owner, const MEProperty& property, void* outValuePtr)
    {
        if (owner == nullptr || outValuePtr == nullptr)
        {
            return false;
        }

        if (property.HasPropertyGetter())
        {
            property.GetPropertyGetter()(owner, outValuePtr);
            return true;
        }

        if (property.GetConstAccessor() == nullptr)
        {
            return false;
        }

        const void* src = property.GetConst(owner);
        if (src == nullptr)
        {
            return false;
        }

        CopyPropertyStorage(property, outValuePtr, src);
        return true;
    }
}

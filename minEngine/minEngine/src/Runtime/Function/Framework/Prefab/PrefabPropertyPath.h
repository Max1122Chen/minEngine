#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Reflection/Reflection.h"

#include <string>
#include <string_view>

namespace minEngine
{
    class MEObject;
    class GameObject;
    class Component;
    class PrefabInstanceRecord;

    struct PrefabResolvedProperty
    {
        MEObject* OwnerObject = nullptr;
        const Reflection::MEClass* OwnerClass = nullptr;
        std::string LeafPropertyName;
        /** Normalized path used as override key (may rewrite component index to Guid form). */
        std::string CanonicalPath;
    };

    class PrefabPropertyPath
    {
    public:
        /**
         * Resolve a property path relative to a Prefab instance object (GO or Component).
         * Supports:
         *   - "m_Name" on GameObject
         *   - "m_Transform.Position" when owner is SceneComponent
         *   - "m_Components/<componentGuid>/m_Transform.Position" when owner is GameObject
         */
        static bool TryResolve(
            MEObject& rootObject,
            std::string_view propertyPath,
            PrefabResolvedProperty& outResolved,
            std::string* outError = nullptr);

        static GUID FindTemplateGuidForInstance(
            const PrefabInstanceRecord& record,
            const GUID& instanceGuid);

        static GUID FindInstanceGuidForTemplate(
            const PrefabInstanceRecord& record,
            const GUID& templateGuid);

        static std::string EncodeBinaryPayload(const std::vector<uint8_t>& buffer);
        static bool DecodeBinaryPayload(std::string_view payload, std::vector<uint8_t>& outBuffer);
    };
}

#include "Runtime/Function/Framework/Prefab/PrefabPropertyPath.h"

#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/PrefabTypes.h"

#include <cctype>
#include <cstdio>

namespace minEngine
{
    namespace
    {
        bool ParseGuidToken(std::string_view token, GUID& outGuid)
        {
            // Accept GUID::ToString form: 8-4-4-4-12 hex
            unsigned int a = 0, b = 0, c = 0, d = 0;
            unsigned long long e = 0;
            if (std::sscanf(
                    std::string(token).c_str(),
                    "%08x-%04x-%04x-%04x-%012llx",
                    &a,
                    &b,
                    &c,
                    &d,
                    &e)
                != 5)
            {
                return false;
            }

            outGuid.High = (static_cast<uint64_t>(a) << 32)
                | (static_cast<uint64_t>(b) << 16)
                | static_cast<uint64_t>(c);
            outGuid.Low = (static_cast<uint64_t>(d) << 48) | e;
            return outGuid.IsValid();
        }

        char NibbleToHex(uint8_t value)
        {
            return static_cast<char>(value < 10 ? ('0' + value) : ('a' + (value - 10)));
        }

        bool HexToNibble(char ch, uint8_t& outValue)
        {
            if (ch >= '0' && ch <= '9')
            {
                outValue = static_cast<uint8_t>(ch - '0');
                return true;
            }
            if (ch >= 'a' && ch <= 'f')
            {
                outValue = static_cast<uint8_t>(10 + (ch - 'a'));
                return true;
            }
            if (ch >= 'A' && ch <= 'F')
            {
                outValue = static_cast<uint8_t>(10 + (ch - 'A'));
                return true;
            }
            return false;
        }
    }

    bool PrefabPropertyPath::TryResolve(
        MEObject& rootObject,
        std::string_view propertyPath,
        PrefabResolvedProperty& outResolved,
        std::string* outError)
    {
        outResolved = PrefabResolvedProperty{};
        if (propertyPath.empty())
        {
            if (outError)
            {
                *outError = "Empty property path.";
            }
            return false;
        }

        constexpr std::string_view kComponentsPrefix = "m_Components/";
        if (propertyPath.rfind(kComponentsPrefix, 0) == 0)
        {
            GameObject* gameObject = dynamic_cast<GameObject*>(&rootObject);
            if (gameObject == nullptr)
            {
                if (outError)
                {
                    *outError = "m_Components/ path requires a GameObject root.";
                }
                return false;
            }

            const std::string_view remainder = propertyPath.substr(kComponentsPrefix.size());
            const size_t slash = remainder.find('/');
            if (slash == std::string_view::npos || slash == 0 || slash + 1 >= remainder.size())
            {
                if (outError)
                {
                    *outError = "Invalid m_Components/<guid>/property path.";
                }
                return false;
            }

            GUID componentGuid;
            if (!ParseGuidToken(remainder.substr(0, slash), componentGuid))
            {
                if (outError)
                {
                    *outError = "Invalid component Guid in property path.";
                }
                return false;
            }

            Component* targetComponent = nullptr;
            for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
            {
                if (component && component->GetGuid() == componentGuid)
                {
                    targetComponent = component.get();
                    break;
                }
            }

            if (targetComponent == nullptr)
            {
                if (outError)
                {
                    *outError = "Component Guid not found on GameObject.";
                }
                return false;
            }

            const std::string_view leafPath = remainder.substr(slash + 1);
            const size_t dot = leafPath.find('.');
            outResolved.OwnerObject = targetComponent;
            outResolved.OwnerClass = targetComponent->GetClass();
            outResolved.LeafPropertyName = std::string(dot == std::string_view::npos ? leafPath : leafPath.substr(0, dot));
            outResolved.CanonicalPath = std::string(propertyPath);
            if (outResolved.OwnerClass == nullptr || outResolved.LeafPropertyName.empty())
            {
                if (outError)
                {
                    *outError = "Failed to resolve component property path.";
                }
                return false;
            }

            // Nested struct paths (m_Transform.Position) are handled by Serializer path walk from OwnerObject.
            if (dot != std::string_view::npos)
            {
                outResolved.LeafPropertyName = std::string(leafPath); // full dotted path from component
            }
            return true;
        }

        outResolved.OwnerObject = &rootObject;
        outResolved.OwnerClass = rootObject.GetClass();
        outResolved.CanonicalPath = std::string(propertyPath);
        outResolved.LeafPropertyName = std::string(propertyPath);
        if (outResolved.OwnerClass == nullptr)
        {
            if (outError)
            {
                *outError = "Root object has no reflected class.";
            }
            return false;
        }

        return true;
    }

    GUID PrefabPropertyPath::FindTemplateGuidForInstance(
        const PrefabInstanceRecord& record,
        const GUID& instanceGuid)
    {
        for (const PrefabObjectMapping& mapping : record.ObjectMappings)
        {
            if (mapping.InstanceGuid == instanceGuid)
            {
                return mapping.TemplateGuid;
            }
        }
        return GUID::Zero();
    }

    GUID PrefabPropertyPath::FindInstanceGuidForTemplate(
        const PrefabInstanceRecord& record,
        const GUID& templateGuid)
    {
        for (const PrefabObjectMapping& mapping : record.ObjectMappings)
        {
            if (mapping.TemplateGuid == templateGuid)
            {
                return mapping.InstanceGuid;
            }
        }
        return GUID::Zero();
    }

    std::string PrefabPropertyPath::EncodeBinaryPayload(const std::vector<uint8_t>& buffer)
    {
        std::string encoded;
        encoded.reserve(4 + buffer.size() * 2);
        encoded.append("bin:");
        for (uint8_t byte : buffer)
        {
            encoded.push_back(NibbleToHex(static_cast<uint8_t>((byte >> 4) & 0x0F)));
            encoded.push_back(NibbleToHex(static_cast<uint8_t>(byte & 0x0F)));
        }
        return encoded;
    }

    bool PrefabPropertyPath::DecodeBinaryPayload(std::string_view payload, std::vector<uint8_t>& outBuffer)
    {
        outBuffer.clear();
        constexpr std::string_view kPrefix = "bin:";
        if (payload.rfind(kPrefix, 0) != 0)
        {
            return false;
        }

        const std::string_view hex = payload.substr(kPrefix.size());
        if ((hex.size() % 2) != 0)
        {
            return false;
        }

        outBuffer.reserve(hex.size() / 2);
        for (size_t index = 0; index + 1 < hex.size(); index += 2)
        {
            uint8_t high = 0;
            uint8_t low = 0;
            if (!HexToNibble(hex[index], high) || !HexToNibble(hex[index + 1], low))
            {
                outBuffer.clear();
                return false;
            }
            outBuffer.push_back(static_cast<uint8_t>((high << 4) | low));
        }

        return true;
    }
}

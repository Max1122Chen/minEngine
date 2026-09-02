#include "Runtime/Core/PropertyPath/PropertyPath.h"

#include "Runtime/Core/Command/CommandResult.h"
#include "Runtime/Core/Reflection/MEEnum.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/MEProperties.h"
#include "Runtime/Core/Reflection/MEProperties.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Reflection/ReflectionDisplayNames.h"
#include "Runtime/Core/Reflection/ReflectionUtils.h"
#include "Runtime/Core/Serialization/BinaryArchive.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

#include <charconv>
#include <cctype>
#include <sstream>

namespace minEngine::Command
{
    namespace
    {
        std::string TrimLiteral(std::string_view literal)
        {
            size_t begin = 0;
            while (begin < literal.size() && std::isspace(static_cast<unsigned char>(literal[begin])))
            {
                ++begin;
            }

            size_t end = literal.size();
            while (end > begin && std::isspace(static_cast<unsigned char>(literal[end - 1])))
            {
                --end;
            }

            return std::string(literal.substr(begin, end - begin));
        }
    }

    PropertyPath::PropertyPath(
        std::string gameObjectName,
        std::string explicitComponentName,
        std::string propertySubPath)
        : m_GameObjectName(std::move(gameObjectName))
        , m_ExplicitComponentName(std::move(explicitComponentName))
        , m_PropertySubPath(std::move(propertySubPath))
    {
    }

    std::optional<PropertyPath> PropertyPath::Parse(std::string_view text)
    {
        if (text.empty())
        {
            return std::nullopt;
        }

        const size_t dotIndex = text.find('.');
        const std::string_view head = dotIndex == std::string_view::npos ? text : text.substr(0, dotIndex);
        std::string propertySubPath;
        if (dotIndex != std::string_view::npos && dotIndex + 1 < text.size())
        {
            propertySubPath = std::string(text.substr(dotIndex + 1));
        }

        if (head.empty())
        {
            return std::nullopt;
        }

        const size_t atIndex = head.find('@');
        std::string gameObjectName;
        std::string explicitComponentName;
        if (atIndex != std::string_view::npos)
        {
            if (atIndex == 0)
            {
                return std::nullopt;
            }

            gameObjectName = std::string(head.substr(0, atIndex));
            explicitComponentName = std::string(head.substr(atIndex + 1));
            if (gameObjectName.empty())
            {
                return std::nullopt;
            }
        }
        else
        {
            gameObjectName = std::string(head);
        }

        return PropertyPath(std::move(gameObjectName), std::move(explicitComponentName), std::move(propertySubPath));
    }

    std::string PropertyPath::GetObjectRef() const
    {
        if (m_ExplicitComponentName.empty())
        {
            return m_GameObjectName;
        }

        return m_GameObjectName + "@" + m_ExplicitComponentName;
    }

    std::string PropertyPath::GetCanonicalPath() const
    {
        if (m_PropertySubPath.empty())
        {
            return GetObjectRef();
        }

        return GetObjectRef() + "." + m_PropertySubPath;
    }

    bool PropertyPath::ComponentTypeMatches(
        const Reflection::MEClass* componentClass,
        std::string_view typeQuery)
    {
        if (componentClass == nullptr || typeQuery.empty())
        {
            return false;
        }

        const std::string_view className = componentClass->GetName();
        if (className == typeQuery)
        {
            return true;
        }

        if (className.size() >= typeQuery.size()
            && className.substr(className.size() - typeQuery.size()) == typeQuery)
        {
            return true;
        }

        const std::string shortName = FormatComponentTypeName(componentClass);
        return shortName == typeQuery;
    }

    std::string PropertyPath::FormatComponentTypeName(const Reflection::MEClass* componentClass)
    {
        if (componentClass == nullptr)
        {
            return {};
        }

        const std::string& className = componentClass->GetName();
        const size_t scopePos = className.rfind("::");
        return scopePos != std::string::npos ? className.substr(scopePos + 2) : className;
    }

    Component* PropertyPath::FindComponentByTypeName(GameObject& gameObject, std::string_view componentTypeName)
    {
        if (componentTypeName.empty())
        {
            return nullptr;
        }

        for (const std::shared_ptr<Component>& component : gameObject.GetAllComponents())
        {
            if (!component)
            {
                continue;
            }

            const Reflection::MEClass* componentClass = component->GetClass();
            if (componentClass != nullptr && ComponentTypeMatches(componentClass, componentTypeName))
            {
                return component.get();
            }
        }

        return nullptr;
    }

    void PropertyPath::CollectShortPathMatches(
        GameObject& gameObject,
        const std::string& propertySubPath,
        std::vector<ResolvedPropertyTarget>& outMatches)
    {
        outMatches.clear();
        if (propertySubPath.empty())
        {
            return;
        }

        const Reflection::MEClass* gameObjectClass = gameObject.GetClass();
        if (gameObjectClass != nullptr
            && TryResolvePropertySubPath(&gameObject, gameObjectClass, propertySubPath))
        {
            ResolvedPropertyTarget target;
            target.OwnerObject = &gameObject;
            target.OwnerClass = gameObjectClass;
            target.PropertySubPath = propertySubPath;
            outMatches.push_back(std::move(target));
        }

        for (const std::shared_ptr<Component>& component : gameObject.GetAllComponents())
        {
            if (!component)
            {
                continue;
            }

            const Reflection::MEClass* componentClass = component->GetClass();
            if (componentClass == nullptr)
            {
                continue;
            }

            if (TryResolvePropertySubPath(component.get(), componentClass, propertySubPath))
            {
                ResolvedPropertyTarget target;
                target.OwnerObject = component.get();
                target.OwnerClass = componentClass;
                target.PropertySubPath = propertySubPath;
                outMatches.push_back(std::move(target));
            }
        }
    }

    PropertyPathResolveStatus PropertyPath::TryResolve(
        const CommandContext& context,
        ResolvedPropertyTarget& outTarget,
        std::vector<std::string>* outAmbiguousCandidates) const
    {
        outTarget = {};
        if (outAmbiguousCandidates != nullptr)
        {
            outAmbiguousCandidates->clear();
        }

        if (context.ActiveScene == nullptr)
        {
            return PropertyPathResolveStatus::NotFound;
        }

        GameObject* gameObject = FindGameObjectByName(context.ActiveScene, m_GameObjectName);
        if (gameObject == nullptr)
        {
            return PropertyPathResolveStatus::NotFound;
        }

        if (m_PropertySubPath.empty())
        {
            outTarget.OwnerObject = gameObject;
            outTarget.OwnerClass = gameObject->GetClass();
            return outTarget.OwnerClass != nullptr ? PropertyPathResolveStatus::Ok
                                                   : PropertyPathResolveStatus::NotFound;
        }

        if (!m_ExplicitComponentName.empty())
        {
            Component* component = FindComponentByTypeName(*gameObject, m_ExplicitComponentName);
            if (component == nullptr)
            {
                return PropertyPathResolveStatus::NotFound;
            }

            const Reflection::MEClass* componentClass = component->GetClass();
            if (componentClass == nullptr
                || !TryResolvePropertySubPath(component, componentClass, m_PropertySubPath))
            {
                return PropertyPathResolveStatus::NotFound;
            }

            outTarget.OwnerObject = component;
            outTarget.OwnerClass = componentClass;
            outTarget.PropertySubPath = m_PropertySubPath;
            return PropertyPathResolveStatus::Ok;
        }

        std::vector<ResolvedPropertyTarget> matches;
        CollectShortPathMatches(*gameObject, m_PropertySubPath, matches);
        if (matches.empty())
        {
            return PropertyPathResolveStatus::NotFound;
        }

        if (matches.size() == 1)
        {
            outTarget = std::move(matches.front());
            return PropertyPathResolveStatus::Ok;
        }

        if (outAmbiguousCandidates != nullptr)
        {
            for (const ResolvedPropertyTarget& match : matches)
            {
                std::string candidatePath = m_GameObjectName;
                if (match.OwnerObject != gameObject)
                {
                    const Reflection::MEClass* ownerClass = match.OwnerClass;
                    if (ownerClass != nullptr)
                    {
                        candidatePath.push_back('@');
                        candidatePath += FormatComponentTypeName(ownerClass);
                    }
                }

                candidatePath.push_back('.');
                candidatePath += m_PropertySubPath;
                outAmbiguousCandidates->push_back(std::move(candidatePath));
            }
        }

        return PropertyPathResolveStatus::Ambiguous;
    }

    bool PropertyPath::Resolve(const CommandContext& context, ResolvedPropertyTarget& outTarget) const
    {
        return TryResolve(context, outTarget, nullptr) == PropertyPathResolveStatus::Ok;
    }

    CommandResult PropertyPath::BuildResolveErrorResult(
        PropertyPathResolveStatus status,
        const std::vector<std::string>* ambiguousCandidates) const
    {
        CommandOutputBuilder builder;
        if (status == PropertyPathResolveStatus::Ambiguous)
        {
            builder.AddLine(
                CommandOutputKind::Error,
                "Error: ambiguous property path '" + GetCanonicalPath() + "'. Use an explicit component path:");
            if (ambiguousCandidates != nullptr)
            {
                for (const std::string& candidate : *ambiguousCandidates)
                {
                    builder.AddSegment(CommandOutputKind::Hint, "  ");
                    builder.AddLine(CommandOutputKind::ListItemName, candidate);
                }
            }
            return builder.BuildError("ambiguous property path");
        }

        builder.AddLine(
            CommandOutputKind::Error,
            "Error: could not resolve property path '" + GetCanonicalPath() + "'");
        return builder.BuildError("property path not found");
    }

    CommandResult PropertyPath::GetValue(const CommandContext& context) const
    {
        std::vector<std::string> ambiguousCandidates;
        ResolvedPropertyTarget target;
        const PropertyPathResolveStatus status = TryResolve(context, target, &ambiguousCandidates);
        if (status != PropertyPathResolveStatus::Ok)
        {
            return BuildResolveErrorResult(status, &ambiguousCandidates);
        }

        if (m_PropertySubPath.empty())
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: get requires a property path.");
            return builder.BuildError("missing property path");
        }

        const std::string valueText =
            FormatPropertyValueAsText(target.OwnerObject, target.OwnerClass, target.PropertySubPath);
        if (valueText.empty())
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: failed to read property value.");
            return builder.BuildError("read failed");
        }

        CommandOutputBuilder builder;
        builder.AddSegment(CommandOutputKind::Path, GetCanonicalPath());
        builder.AddSegment(CommandOutputKind::Muted, " = ");
        builder.AddSegment(CommandOutputKind::ValueLiteral, valueText);
        builder.NewLine();
        return builder.BuildOk(valueText);
    }

    std::string PropertyPath::FormatPropertyTypeName(const Reflection::MEProperty& property)
    {
        if (property.GetCategory() == Reflection::MEPropertyCategory::Primitive)
        {
            const auto* primitiveProperty = static_cast<const Reflection::MEPrimitiveProperty*>(&property);
            if (primitiveProperty->IsEnum())
            {
                if (const Reflection::MEEnum* enumType = primitiveProperty->GetEnum())
                {
                    return enumType->GetName();
                }

                return "enum";
            }

            const std::string& primitiveTypeName = primitiveProperty->primitiveTypeName;
            if (primitiveTypeName == Reflection::GetPrimitiveName<bool>())
            {
                return "bool";
            }

            if (primitiveTypeName == Reflection::GetPrimitiveName<float>())
            {
                return "float";
            }

            if (primitiveTypeName == Reflection::GetPrimitiveName<double>())
            {
                return "double";
            }

            if (primitiveTypeName == Reflection::GetPrimitiveName<int32_t>()
                || primitiveTypeName == Reflection::GetPrimitiveName<int64_t>())
            {
                return "int";
            }

            if (primitiveTypeName == Reflection::GetPrimitiveName<uint32_t>()
                || primitiveTypeName == Reflection::GetPrimitiveName<uint64_t>())
            {
                return "uint";
            }

            if (primitiveTypeName == Reflection::GetPrimitiveName<std::string>())
            {
                return "string";
            }

            return primitiveTypeName;
        }

        if (property.GetCategory() == Reflection::MEPropertyCategory::Object)
        {
            return "object";
        }

        if (property.GetCategory() == Reflection::MEPropertyCategory::ObjectPtr)
        {
            return "objectptr";
        }

        if (property.GetCategory() == Reflection::MEPropertyCategory::Array)
        {
            return "array";
        }

        return "property";
    }

    GameObject* PropertyPath::FindGameObjectByName(Scene* scene, std::string_view objectRef)
    {
        if (scene == nullptr || objectRef.empty())
        {
            return nullptr;
        }

        for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
        {
            if (gameObject && gameObject->GetName() == objectRef)
            {
                return gameObject.get();
            }
        }

        return nullptr;
    }

    bool PropertyPath::TryResolvePropertySubPath(
        void* ownerObject,
        const Reflection::MEClass* ownerClass,
        const std::string& propertySubPath)
    {
        if (ownerObject == nullptr || ownerClass == nullptr || propertySubPath.empty())
        {
            return false;
        }

        std::vector<uint8_t> buffer;
        const Serialization::SerializeResult result = Serialization::Serializer::SerializePropertyByPathToBuffer(
            ownerObject,
            ownerClass,
            propertySubPath,
            buffer);
        return result.ok;
    }

    bool PropertyPath::ResolvePropertySubPathOnGameObject(
        GameObject& gameObject,
        const std::string& propertySubPath,
        ResolvedPropertyTarget& outTarget)
    {
        const Reflection::MEClass* gameObjectClass = gameObject.GetClass();
        if (gameObjectClass == nullptr)
        {
            return false;
        }

        if (TryResolvePropertySubPath(&gameObject, gameObjectClass, propertySubPath))
        {
            outTarget.OwnerObject = &gameObject;
            outTarget.OwnerClass = gameObjectClass;
            outTarget.PropertySubPath = propertySubPath;
            return true;
        }

        for (const std::shared_ptr<Component>& component : gameObject.GetAllComponents())
        {
            if (!component)
            {
                continue;
            }

            const Reflection::MEClass* componentClass = component->GetClass();
            if (componentClass == nullptr)
            {
                continue;
            }

            if (TryResolvePropertySubPath(component.get(), componentClass, propertySubPath))
            {
                outTarget.OwnerObject = component.get();
                outTarget.OwnerClass = componentClass;
                outTarget.PropertySubPath = propertySubPath;
                return true;
            }
        }

        return false;
    }

    const Reflection::MEProperty* PropertyPath::FindPropertyInHierarchy(
        const Reflection::MEClass* ownerClass,
        std::string_view propertyName)
    {
        if (ownerClass == nullptr || propertyName.empty())
        {
            return nullptr;
        }

        const Reflection::MEProperty* foundProperty = nullptr;
        Reflection::ReflectionSystem::Get().ForEachPropertyInHierarchy(
            ownerClass->GetName(),
            [&](const Reflection::MEProperty& property) -> bool
            {
                if (property.GetName() == propertyName)
                {
                    foundProperty = &property;
                    return false;
                }

                return true;
            });
        return foundProperty;
    }

    bool PropertyPath::WalkToLeafOwner(
        void* ownerObject,
        const Reflection::MEClass* ownerClass,
        const std::string& propertySubPath,
        void*& outLeafOwner,
        const Reflection::MEClass*& outLeafOwnerClass,
        std::string& outLeafPropertyName,
        std::string& outError)
    {
        outLeafOwner = ownerObject;
        outLeafOwnerClass = ownerClass;
        outLeafPropertyName.clear();
        outError.clear();

        if (ownerObject == nullptr || ownerClass == nullptr)
        {
            outError = "Property path owner is null.";
            return false;
        }

        if (propertySubPath.empty())
        {
            return true;
        }

        size_t segmentStart = 0;
        std::vector<std::string_view> segments;
        while (segmentStart < propertySubPath.size())
        {
            const size_t dotIndex = propertySubPath.find('.', segmentStart);
            const size_t segmentEnd = dotIndex == std::string::npos ? propertySubPath.size() : dotIndex;
            const std::string_view segment =
                std::string_view(propertySubPath).substr(segmentStart, segmentEnd - segmentStart);
            if (segment.empty())
            {
                outError = "Invalid property path segment.";
                return false;
            }

            segments.push_back(segment);
            if (dotIndex == std::string::npos)
            {
                break;
            }

            segmentStart = dotIndex + 1;
        }

        if (segments.empty())
        {
            outError = "Invalid property path.";
            return false;
        }

        outLeafPropertyName = std::string(segments.back());
        if (segments.size() == 1)
        {
            return true;
        }

        void* currentOwnerObject = ownerObject;
        const Reflection::MEClass* currentOwnerClass = ownerClass;
        for (size_t segmentIndex = 0; segmentIndex + 1 < segments.size(); ++segmentIndex)
        {
            const Reflection::MEProperty* property = FindPropertyInHierarchy(currentOwnerClass, segments[segmentIndex]);
            if (property == nullptr)
            {
                outError = "Property path segment not found: " + std::string(segments[segmentIndex]);
                return false;
            }

            if (property->GetCategory() != Reflection::MEPropertyCategory::Object)
            {
                outError = "Property path contains a non-object segment: " + std::string(segments[segmentIndex]);
                return false;
            }

            const Reflection::MEObjectProperty& objectProperty =
                static_cast<const Reflection::MEObjectProperty&>(*property);
            const Reflection::MEClass* valueClass = objectProperty.GetValueClass();
            if (valueClass == nullptr)
            {
                outError = "Property path value class is unresolved.";
                return false;
            }

            if (property->GetConstAccessor() == nullptr)
            {
                outError = "Property path segment is not readable.";
                return false;
            }

            void* nextObjectPtr = const_cast<void*>(property->GetConst(currentOwnerObject));
            if (nextObjectPtr == nullptr)
            {
                outError = "Property path intermediate object is null.";
                return false;
            }

            currentOwnerObject = nextObjectPtr;
            currentOwnerClass = valueClass;
        }

        outLeafOwner = currentOwnerObject;
        outLeafOwnerClass = currentOwnerClass;
        return true;
    }

    bool PropertyPath::IsPropertyVisible(const Reflection::MEProperty& property)
    {
        return !property.HasSpecifier(Reflection::PropertySpecifier::Invisible);
    }

    bool PropertyPath::IsPropertyWritable(const Reflection::MEProperty& property)
    {
        if (!IsPropertyVisible(property))
        {
            return false;
        }

        if (property.HasSpecifier(Reflection::PropertySpecifier::Transient))
        {
            return false;
        }

        if (property.GetMutableAccessor() == nullptr)
        {
            return false;
        }

        const std::string* readOnlyMetadata = property.FindMetadata("ReadOnly");
        if (readOnlyMetadata != nullptr
            && (*readOnlyMetadata == "true" || *readOnlyMetadata == "1" || *readOnlyMetadata == "True"))
        {
            return false;
        }

        return true;
    }

    std::string PropertyPath::FormatPropertyValueAsText(
        void* ownerObject,
        const Reflection::MEClass* ownerClass,
        const std::string& propertySubPath)
    {
        void* leafOwner = nullptr;
        const Reflection::MEClass* leafOwnerClass = nullptr;
        std::string leafPropertyName;
        std::string walkError;
        if (!WalkToLeafOwner(
                ownerObject,
                ownerClass,
                propertySubPath,
                leafOwner,
                leafOwnerClass,
                leafPropertyName,
                walkError))
        {
            return {};
        }

        if (propertySubPath.empty())
        {
            return leafOwnerClass != nullptr ? leafOwnerClass->GetName() : std::string{};
        }

        Serialization::JsonWriterArchive writer;
        const Serialization::SerializeResult serializeResult = Serialization::Serializer::SerializeProperty(
            leafOwner,
            leafOwnerClass,
            leafPropertyName,
            writer);
        if (!serializeResult.ok)
        {
            return {};
        }

        const Json& root = writer.GetRoot();
        if (root.is_string())
        {
            return root.get<std::string>();
        }

        if (root.is_boolean())
        {
            return root.get<bool>() ? "true" : "false";
        }

        if (root.is_number_integer())
        {
            return std::to_string(root.get<int64_t>());
        }

        if (root.is_number_unsigned())
        {
            return std::to_string(root.get<uint64_t>());
        }

        if (root.is_number_float())
        {
            std::ostringstream stream;
            stream << root.get<double>();
            return stream.str();
        }

        return root.dump();
    }

    bool PropertyPath::WritePrimitiveLiteralToBuffer(
        const Reflection::MEProperty& property,
        std::string_view literal,
        std::vector<uint8_t>& outBuffer,
        std::string& outError)
    {
        outBuffer.clear();
        outError.clear();

        if (property.GetCategory() != Reflection::MEPropertyCategory::Primitive)
        {
            outError = "set only supports primitive properties in this slice.";
            return false;
        }

        const Reflection::MEPrimitiveProperty& primitiveProperty =
            static_cast<const Reflection::MEPrimitiveProperty&>(property);
        const std::string& primitiveTypeName = primitiveProperty.primitiveTypeName;

        Serialization::BinaryWriterArchive writer;
        const std::string literalText = TrimLiteral(literal);

        if (primitiveProperty.IsEnum())
        {
            const Reflection::MEEnum* enumType = primitiveProperty.GetEnum();
            if (enumType == nullptr)
            {
                outError = "enum type is unresolved.";
                return false;
            }

            const Reflection::MEEnumEntry* enumEntry = enumType->FindByName(literalText);
            if (enumEntry == nullptr)
            {
                outError = "expected enum '" + enumType->GetName() + "', got '" + literalText + "'";
                return false;
            }

            if (!writer.WriteInt64(enumEntry->value))
            {
                outError = "Failed to encode enum literal.";
                return false;
            }

            outBuffer = writer.TakeBuffer();
            return true;
        }

        if (primitiveTypeName == Reflection::GetPrimitiveName<bool>())
        {
            bool value = false;
            if (literalText == "true" || literalText == "1" || literalText == "True" || literalText == "TRUE")
            {
                value = true;
            }
            else if (literalText == "false" || literalText == "0" || literalText == "False" || literalText == "FALSE")
            {
                value = false;
            }
            else
            {
                outError = "expected bool literal (true/false), got '" + literalText + "'";
                return false;
            }

            if (!writer.WriteBool(value))
            {
                outError = "Failed to encode bool literal.";
                return false;
            }
        }
        else if (primitiveTypeName == Reflection::GetPrimitiveName<float>())
        {
            float value = 0.0f;
            try
            {
                value = std::stof(literalText);
            }
            catch (const std::exception&)
            {
                outError = "expected float, got '" + literalText + "'";
                return false;
            }

            if (!writer.WriteDouble(static_cast<double>(value)))
            {
                outError = "Failed to encode float literal.";
                return false;
            }
        }
        else if (primitiveTypeName == Reflection::GetPrimitiveName<double>())
        {
            double value = 0.0;
            try
            {
                value = std::stod(literalText);
            }
            catch (const std::exception&)
            {
                outError = "expected double, got '" + literalText + "'";
                return false;
            }

            if (!writer.WriteDouble(value))
            {
                outError = "Failed to encode double literal.";
                return false;
            }
        }
        else if (primitiveTypeName == Reflection::GetPrimitiveName<int32_t>()
                 || primitiveTypeName == Reflection::GetPrimitiveName<int64_t>())
        {
            int64_t value = 0;
            const char* begin = literalText.data();
            const char* end = literalText.data() + literalText.size();
            const std::from_chars_result parseResult = std::from_chars(begin, end, value);
            if (parseResult.ec != std::errc() || parseResult.ptr != end)
            {
                outError = "expected integer, got '" + literalText + "'";
                return false;
            }

            if (!writer.WriteInt64(value))
            {
                outError = "Failed to encode integer literal.";
                return false;
            }
        }
        else if (primitiveTypeName == Reflection::GetPrimitiveName<uint32_t>()
                 || primitiveTypeName == Reflection::GetPrimitiveName<uint64_t>())
        {
            uint64_t value = 0;
            const char* begin = literalText.data();
            const char* end = literalText.data() + literalText.size();
            const std::from_chars_result parseResult = std::from_chars(begin, end, value);
            if (parseResult.ec != std::errc() || parseResult.ptr != end)
            {
                outError = "expected unsigned integer, got '" + literalText + "'";
                return false;
            }

            if (!writer.WriteUInt64(value))
            {
                outError = "Failed to encode unsigned integer literal.";
                return false;
            }
        }
        else if (primitiveTypeName == Reflection::GetPrimitiveName<std::string>())
        {
            if (!writer.WriteString(literalText))
            {
                outError = "Failed to encode string literal.";
                return false;
            }
        }
        else
        {
            outError = "Unsupported primitive type for set: " + primitiveTypeName;
            return false;
        }

        outBuffer = writer.TakeBuffer();
        return true;
    }

    void PropertyPath::AppendInspectProperties(
        CommandOutputBuilder& builder,
        void* ownerObject,
        const Reflection::MEClass* ownerClass,
        int indentLevel)
    {
        if (ownerObject == nullptr || ownerClass == nullptr)
        {
            return;
        }

        const std::string indent(static_cast<size_t>(indentLevel) * 2, ' ');
        Reflection::ReflectionSystem::Get().ForEachPropertyInHierarchy(
            ownerClass->GetName(),
            [&](const Reflection::MEProperty& property) -> bool
            {
                if (!IsPropertyVisible(property))
                {
                    return true;
                }

                if (property.GetCategory() == Reflection::MEPropertyCategory::Array
                    || property.GetCategory() == Reflection::MEPropertyCategory::ObjectPtr)
                {
                    return true;
                }

                builder.AddSegment(CommandOutputKind::InspectKey, indent + Reflection::GetPropertyDisplayName(property));
                builder.AddSegment(CommandOutputKind::Muted, ": ");

                if (property.GetCategory() == Reflection::MEPropertyCategory::Primitive)
                {
                    const std::string valueText = FormatPropertyValueAsText(ownerObject, ownerClass, property.GetName());
                    builder.AddSegment(CommandOutputKind::InspectValue, valueText.empty() ? "<unavailable>" : valueText);
                    builder.NewLine();
                    return true;
                }

                if (property.GetCategory() == Reflection::MEPropertyCategory::Object)
                {
                    builder.AddSegment(CommandOutputKind::InspectType, "<object>");
                    builder.NewLine();
                    return true;
                }

                return true;
            });
    }

    CommandResult PropertyPath::BuildSetValueSuccessResult(const CommandContext& context) const
    {
        ResolvedPropertyTarget target;
        if (!Resolve(context, target))
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: could not resolve property path after set.");
            return builder.BuildError("property path not found");
        }

        const std::string valueText =
            FormatPropertyValueAsText(target.OwnerObject, target.OwnerClass, target.PropertySubPath);
        CommandOutputBuilder builder;
        builder.AddSegment(CommandOutputKind::Path, GetCanonicalPath());
        builder.AddSegment(CommandOutputKind::Muted, " = ");
        builder.AddSegment(CommandOutputKind::ValueLiteral, valueText);
        builder.NewLine();
        return builder.BuildOk(valueText);
    }

    bool PropertyPath::TryBuildSetTransaction(
        const CommandContext& context,
        std::string_view literal,
        PropertySetTransaction& outTransaction,
        CommandResult& outError,
        const Serialization::SerializerOptions* serializerOptions) const
    {
        outTransaction = {};
        outError = CommandResult::MakeError("set failed");

        if (literal.empty())
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: set requires a value literal.");
            outError = builder.BuildError("missing value");
            return false;
        }

        std::vector<std::string> ambiguousCandidates;
        ResolvedPropertyTarget target;
        const PropertyPathResolveStatus status = TryResolve(context, target, &ambiguousCandidates);
        if (status != PropertyPathResolveStatus::Ok)
        {
            outError = BuildResolveErrorResult(status, &ambiguousCandidates);
            return false;
        }

        void* leafOwner = nullptr;
        const Reflection::MEClass* leafOwnerClass = nullptr;
        std::string leafPropertyName;
        std::string walkError;
        if (!WalkToLeafOwner(
                target.OwnerObject,
                target.OwnerClass,
                target.PropertySubPath,
                leafOwner,
                leafOwnerClass,
                leafPropertyName,
                walkError))
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: " + walkError);
            outError = builder.BuildError(walkError);
            return false;
        }

        const Reflection::MEProperty* leafProperty = FindPropertyInHierarchy(leafOwnerClass, leafPropertyName);
        if (leafProperty == nullptr)
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: leaf property not found.");
            outError = builder.BuildError("leaf property not found");
            return false;
        }

        if (!IsPropertyWritable(*leafProperty))
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: property is read-only.");
            outError = builder.BuildError("read-only property");
            return false;
        }

        const Serialization::SerializerOptions options =
            serializerOptions != nullptr ? *serializerOptions : Serialization::SerializerOptions{};

        if (!Serialization::Serializer::SerializePropertyByPathToBuffer(
                target.OwnerObject,
                target.OwnerClass,
                target.PropertySubPath,
                outTransaction.BeforeValue,
                options)
                .ok)
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: failed to read current property value.");
            outError = builder.BuildError("read failed");
            return false;
        }

        std::string encodeError;
        if (!WritePrimitiveLiteralToBuffer(*leafProperty, literal, outTransaction.AfterValue, encodeError))
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: " + encodeError);
            outError = builder.BuildError(encodeError);
            return false;
        }

        auto* ownerObject = static_cast<MEObject*>(target.OwnerObject);
        if (ownerObject == nullptr || target.OwnerClass == nullptr)
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: invalid property owner.");
            outError = builder.BuildError("invalid owner");
            return false;
        }

        outTransaction.OwnerGuid = ownerObject->GetGuid();
        outTransaction.OwnerClassName = target.OwnerClass->GetName();
        outTransaction.PropertySubPath = target.PropertySubPath;
        outError = CommandResult::MakeOk();
        return true;
    }

    CommandResult PropertyPath::SetValue(const CommandContext& context, std::string_view literal) const
    {
        PropertySetTransaction transaction;
        CommandResult buildError;
        if (!TryBuildSetTransaction(context, literal, transaction, buildError))
        {
            return buildError;
        }

        std::shared_ptr<MEObject> ownerObject = ObjectManager::Get().FindObject(transaction.OwnerGuid);
        if (!ownerObject)
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: property owner not found.");
            return builder.BuildError("owner not found");
        }

        const Reflection::MEClass* ownerClass =
            Reflection::ReflectionSystem::Get().FindClass(transaction.OwnerClassName);
        if (ownerClass == nullptr)
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: property owner class not found.");
            return builder.BuildError("owner class not found");
        }

        std::vector<Serialization::PendingObjectRef> pendingRefs;
        const Serialization::SerializeResult deserializeResult =
            Serialization::Serializer::DeserializePropertyByPathFromBuffer(
                ownerObject.get(),
                ownerClass,
                transaction.PropertySubPath,
                transaction.AfterValue,
                pendingRefs);
        if (!deserializeResult.ok)
        {
            CommandOutputBuilder builder;
            builder.AddLine(
                CommandOutputKind::Error,
                "Error: failed to set property: " + deserializeResult.message);
            return builder.BuildError(deserializeResult.message);
        }

        return BuildSetValueSuccessResult(context);
    }

    CommandResult PropertyPath::Inspect(const CommandContext& context) const
    {
        if (context.ActiveScene == nullptr)
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: no active scene.");
            return builder.BuildError("no active scene");
        }

        GameObject* gameObject = FindGameObjectByName(context.ActiveScene, m_GameObjectName);
        if (gameObject == nullptr)
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: object not found '" + m_GameObjectName + "'");
            return builder.BuildError("object not found");
        }

        const Reflection::MEClass* gameObjectClass = gameObject->GetClass();
        if (gameObjectClass == nullptr)
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: object class unresolved.");
            return builder.BuildError("class unresolved");
        }

        CommandOutputBuilder builder;
        builder.AddSegment(CommandOutputKind::InspectHeader, gameObject->GetName());
        builder.AddSegment(CommandOutputKind::Muted, " : ");
        builder.AddSegment(CommandOutputKind::InspectType, gameObjectClass->GetName());
        builder.NewLine();

        if (!m_PropertySubPath.empty())
        {
            std::vector<std::string> ambiguousCandidates;
            ResolvedPropertyTarget target;
            const PropertyPathResolveStatus status = TryResolve(context, target, &ambiguousCandidates);
            if (status != PropertyPathResolveStatus::Ok)
            {
                return BuildResolveErrorResult(status, &ambiguousCandidates);
            }

            void* leafOwner = nullptr;
            const Reflection::MEClass* leafOwnerClass = nullptr;
            std::string leafPropertyName;
            std::string walkError;
            if (!WalkToLeafOwner(
                    target.OwnerObject,
                    target.OwnerClass,
                    target.PropertySubPath,
                    leafOwner,
                    leafOwnerClass,
                    leafPropertyName,
                    walkError))
            {
                builder.AddLine(CommandOutputKind::Error, "Error: " + walkError);
                return builder.BuildError(walkError);
            }

            const Reflection::MEProperty* leafProperty = FindPropertyInHierarchy(leafOwnerClass, leafPropertyName);
            if (leafProperty == nullptr)
            {
                builder.AddLine(CommandOutputKind::Error, "Error: leaf property not found.");
                return builder.BuildError("leaf property not found");
            }

            if (leafProperty->GetCategory() == Reflection::MEPropertyCategory::Object)
            {
                builder.AddLine(CommandOutputKind::InspectSection, Reflection::GetPropertyDisplayName(*leafProperty));
                AppendInspectProperties(builder, leafOwner, leafOwnerClass, 1);
                return builder.BuildOk();
            }

            const std::string valueText =
                FormatPropertyValueAsText(target.OwnerObject, target.OwnerClass, target.PropertySubPath);
            builder.AddSegment(CommandOutputKind::InspectKey, Reflection::GetPropertyDisplayName(*leafProperty));
            builder.AddSegment(CommandOutputKind::Muted, ": ");
            builder.AddSegment(CommandOutputKind::InspectValue, valueText.empty() ? "<unavailable>" : valueText);
            builder.NewLine();
            return builder.BuildOk();
        }

        if (!m_ExplicitComponentName.empty())
        {
            Component* component = FindComponentByTypeName(*gameObject, m_ExplicitComponentName);
            if (component == nullptr)
            {
                builder.AddLine(
                    CommandOutputKind::Error,
                    "Error: component not found '" + m_ExplicitComponentName + "'");
                return builder.BuildError("component not found");
            }

            const Reflection::MEClass* componentClass = component->GetClass();
            if (componentClass == nullptr)
            {
                builder.AddLine(CommandOutputKind::Error, "Error: component class unresolved.");
                return builder.BuildError("class unresolved");
            }

            builder.AddLine(CommandOutputKind::InspectSection, componentClass->GetName());
            AppendInspectProperties(builder, component, componentClass, 1);
            return builder.BuildOk();
        }

        builder.AddLine(CommandOutputKind::InspectSection, "GameObject");
        AppendInspectProperties(builder, gameObject, gameObjectClass, 1);

        for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
        {
            if (!component)
            {
                continue;
            }

            const Reflection::MEClass* componentClass = component->GetClass();
            if (componentClass == nullptr)
            {
                continue;
            }

            builder.AddLine(CommandOutputKind::InspectSection, componentClass->GetName());
            AppendInspectProperties(builder, component.get(), componentClass, 1);
        }

        return builder.BuildOk();
    }

    bool PropertyPath::TryResolveLeafProperty(
        const CommandContext& context,
        const Reflection::MEProperty*& outProperty) const
    {
        outProperty = nullptr;

        if (m_PropertySubPath.empty())
        {
            return false;
        }

        ResolvedPropertyTarget target;
        if (!Resolve(context, target))
        {
            return false;
        }

        void* leafOwner = nullptr;
        const Reflection::MEClass* leafOwnerClass = nullptr;
        std::string leafPropertyName;
        std::string walkError;
        if (!WalkToLeafOwner(
                target.OwnerObject,
                target.OwnerClass,
                target.PropertySubPath,
                leafOwner,
                leafOwnerClass,
                leafPropertyName,
                walkError))
        {
            return false;
        }

        outProperty = FindPropertyInHierarchy(leafOwnerClass, leafPropertyName);
        return outProperty != nullptr;
    }
}

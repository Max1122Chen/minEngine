#pragma once

#include "Runtime/Core/Command/CommandContext.h"
#include "Runtime/Core/Command/CompletionTypes.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine::Reflection
{
    class MEEnum;
    class MEProperty;
}

namespace minEngine::Command
{
    enum class PropertySetValueKind : uint8_t
    {
        Unknown,
        Bool,
        SignedInteger,
        UnsignedInteger,
        Float,
        Double,
        String,
        Enum,
    };

    enum class PropertyValueValidationState : uint8_t
    {
        None,
        Valid,
        Partial,
        Invalid,
    };

    struct PropertySetValueInfo
    {
        PropertySetValueKind Kind = PropertySetValueKind::Unknown;
        const Reflection::MEEnum* EnumType = nullptr;
        bool bWritable = false;
        bool bHasClampMin = false;
        bool bHasClampMax = false;
        double ClampMin = 0.0;
        double ClampMax = 0.0;
    };

    struct SetValuePhase
    {
        bool bActive = false;
        std::string PropertyPathText;
        std::string_view ValuePrefix;
    };

    struct PropertyValueValidation
    {
        PropertyValueValidationState State = PropertyValueValidationState::None;
        std::string Message;
        std::vector<std::string> Suggestions;
        PropertySetValueInfo ValueInfo{};
    };

    class SetValueValidation
    {
    public:
        static SetValuePhase ParseValuePhase(std::string_view line);

        static bool TryGetValueInfo(
            const CommandContext& context,
            std::string_view propertyPathText,
            PropertySetValueInfo& outInfo);

        static PropertyValueValidation ValidateValuePrefix(
            const PropertySetValueInfo& info,
            std::string_view valuePrefix);

        static PropertyValueValidation ValidateInputLine(const CommandContext& context, std::string_view line);

        static PropertyValueValidation ValidateSetLiteral(
            const CommandContext& context,
            std::string_view propertyPathText,
            std::string_view valueLiteral);

        static void PopulatePropertyConstraints(
            const Reflection::MEProperty& property,
            PropertySetValueInfo& inOutInfo);

        static std::optional<std::string> ValidateNumericConstraints(
            const PropertySetValueInfo& info,
            double numericValue);

        static std::vector<CompletionItem> CompleteValue(const CommandContext& context, const SetValuePhase& phase);
    };
}

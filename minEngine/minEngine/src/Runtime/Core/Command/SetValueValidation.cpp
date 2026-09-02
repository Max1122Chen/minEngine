#include "Runtime/Core/Command/SetValueValidation.h"

#include "Runtime/Core/PropertyPath/PropertyPath.h"
#include "Runtime/Core/Reflection/MEEnum.h"
#include "Runtime/Core/Reflection/MEProperties.h"
#include "Runtime/Core/Reflection/ReflectionUtils.h"

#include <charconv>
#include <cctype>

namespace minEngine::Command
{
    namespace
    {
        std::vector<std::string> TokenizeLine(std::string_view line)
        {
            std::vector<std::string> tokens;
            std::string currentToken;
            for (const char character : line)
            {
                if (character == ' ' || character == '\t')
                {
                    if (!currentToken.empty())
                    {
                        tokens.push_back(currentToken);
                        currentToken.clear();
                    }
                    continue;
                }

                currentToken.push_back(character);
            }

            if (!currentToken.empty())
            {
                tokens.push_back(currentToken);
            }

            return tokens;
        }

        bool EndsWithPartialToken(std::string_view line)
        {
            if (line.empty())
            {
                return false;
            }

            return line.back() != ' ' && line.back() != '\t';
        }

        std::string_view CurrentToken(std::string_view line)
        {
            const size_t lastSpace = line.find_last_of(" \t");
            if (lastSpace == std::string_view::npos)
            {
                return line;
            }

            return line.substr(lastSpace + 1);
        }

        bool IsSetValuePhase(const std::vector<std::string>& tokens, bool hasPartialToken)
        {
            if (tokens.size() >= 3)
            {
                return true;
            }

            return tokens.size() == 2 && !hasPartialToken;
        }

        std::string_view ExtractSetValuePrefix(
            const std::vector<std::string>& tokens,
            bool hasPartialToken,
            std::string_view currentToken)
        {
            if (tokens.size() < 2)
            {
                return {};
            }

            size_t valueTokenIndex = 2;
            if (tokens.size() > valueTokenIndex && tokens[valueTokenIndex] == "=")
            {
                ++valueTokenIndex;
            }

            if (tokens.size() > valueTokenIndex)
            {
                if (hasPartialToken)
                {
                    return currentToken;
                }

                return {};
            }

            if (hasPartialToken && valueTokenIndex == 2)
            {
                return currentToken;
            }

            return {};
        }

        bool EqualsIgnoreCase(std::string_view left, std::string_view right)
        {
            if (left.size() != right.size())
            {
                return false;
            }

            for (size_t index = 0; index < left.size(); ++index)
            {
                if (std::tolower(static_cast<unsigned char>(left[index]))
                    != std::tolower(static_cast<unsigned char>(right[index])))
                {
                    return false;
                }
            }

            return true;
        }

        bool StartsWithIgnoreCase(std::string_view text, std::string_view prefix)
        {
            if (prefix.size() > text.size())
            {
                return false;
            }

            return EqualsIgnoreCase(text.substr(0, prefix.size()), prefix);
        }

        bool IsBoolLiteral(std::string_view literal, bool& outValue)
        {
            if (EqualsIgnoreCase(literal, "true") || literal == "1")
            {
                outValue = true;
                return true;
            }

            if (EqualsIgnoreCase(literal, "false") || literal == "0")
            {
                outValue = false;
                return true;
            }

            return false;
        }

        bool IsBoolLiteralPrefix(std::string_view prefix)
        {
            static constexpr std::string_view kBoolPrefixes[] = {"true", "false", "1", "0"};
            for (const std::string_view candidate : kBoolPrefixes)
            {
                if (StartsWithIgnoreCase(candidate, prefix))
                {
                    return true;
                }
            }

            return false;
        }

        bool IsNumericPrefix(std::string_view prefix)
        {
            if (prefix.empty())
            {
                return false;
            }

            bool sawDigit = false;
            size_t index = 0;
            if (prefix[index] == '+' || prefix[index] == '-')
            {
                ++index;
            }

            while (index < prefix.size())
            {
                const char character = prefix[index];
                if (std::isdigit(static_cast<unsigned char>(character)))
                {
                    sawDigit = true;
                    ++index;
                    continue;
                }

                if (character == '.')
                {
                    ++index;
                    continue;
                }

                if ((character == 'e' || character == 'E') && sawDigit)
                {
                    ++index;
                    if (index < prefix.size() && (prefix[index] == '+' || prefix[index] == '-'))
                    {
                        ++index;
                    }
                    continue;
                }

                return false;
            }

            return sawDigit || prefix == "-" || prefix == "+";
        }

        bool IsIncompleteNumericLiteral(std::string_view literal)
        {
            if (literal.empty())
            {
                return false;
            }

            const char lastCharacter = literal.back();
            if (lastCharacter == '.')
            {
                return true;
            }

            if (lastCharacter == 'e' || lastCharacter == 'E')
            {
                return true;
            }

            if (lastCharacter == '+' || lastCharacter == '-')
            {
                return literal.size() == 1
                    || literal[literal.size() - 2] == 'e' || literal[literal.size() - 2] == 'E';
            }

            return false;
        }

        bool TryParseSignedInteger(std::string_view literal, int64_t& outValue)
        {
            const std::string literalText(literal);
            const char* begin = literalText.data();
            const char* end = literalText.data() + literalText.size();
            const std::from_chars_result parseResult = std::from_chars(begin, end, outValue);
            return parseResult.ec == std::errc() && parseResult.ptr == end;
        }

        bool TryParseUnsignedInteger(std::string_view literal, uint64_t& outValue)
        {
            const std::string literalText(literal);
            const char* begin = literalText.data();
            const char* end = literalText.data() + literalText.size();
            const std::from_chars_result parseResult = std::from_chars(begin, end, outValue);
            return parseResult.ec == std::errc() && parseResult.ptr == end;
        }

        bool TryParseFloat(std::string_view literal, double& outValue)
        {
            try
            {
                size_t processed = 0;
                outValue = std::stod(std::string(literal), &processed);
                return processed == literal.size();
            }
            catch (const std::exception&)
            {
                return false;
            }
        }

        PropertySetValueKind ClassifyPrimitiveProperty(const Reflection::MEPrimitiveProperty& primitiveProperty)
        {
            if (primitiveProperty.IsEnum())
            {
                return PropertySetValueKind::Enum;
            }

            const std::string& primitiveTypeName = primitiveProperty.primitiveTypeName;
            if (primitiveTypeName == Reflection::GetPrimitiveName<bool>())
            {
                return PropertySetValueKind::Bool;
            }

            if (primitiveTypeName == Reflection::GetPrimitiveName<float>())
            {
                return PropertySetValueKind::Float;
            }

            if (primitiveTypeName == Reflection::GetPrimitiveName<double>())
            {
                return PropertySetValueKind::Double;
            }

            if (primitiveTypeName == Reflection::GetPrimitiveName<int32_t>()
                || primitiveTypeName == Reflection::GetPrimitiveName<int64_t>())
            {
                return PropertySetValueKind::SignedInteger;
            }

            if (primitiveTypeName == Reflection::GetPrimitiveName<uint32_t>()
                || primitiveTypeName == Reflection::GetPrimitiveName<uint64_t>())
            {
                return PropertySetValueKind::UnsignedInteger;
            }

            if (primitiveTypeName == Reflection::GetPrimitiveName<std::string>())
            {
                return PropertySetValueKind::String;
            }

            return PropertySetValueKind::Unknown;
        }

        void AppendBoolCompletions(std::string_view prefix, std::vector<CompletionItem>& outItems)
        {
            static constexpr std::string_view kBoolValues[] = {"true", "false"};
            for (const std::string_view boolValue : kBoolValues)
            {
                if (!prefix.empty() && !StartsWithIgnoreCase(boolValue, prefix))
                {
                    continue;
                }

                CompletionItem item;
                item.Label = std::string(boolValue);
                item.InsertText = std::string(boolValue);
                item.Description = "bool";
                item.Kind = CompletionKind::ValueLiteral;
                outItems.push_back(std::move(item));
            }
        }

        void AppendEnumCompletions(
            const Reflection::MEEnum& enumType,
            std::string_view prefix,
            std::vector<CompletionItem>& outItems)
        {
            for (const Reflection::MEEnumEntry& entry : enumType.GetEntries())
            {
                if (!prefix.empty() && !StartsWithIgnoreCase(entry.name, prefix))
                {
                    continue;
                }

                CompletionItem item;
                item.Label = entry.name;
                item.InsertText = entry.name;
                item.Description = enumType.GetName();
                item.Kind = CompletionKind::EnumValue;
                outItems.push_back(std::move(item));
            }
        }
    }

    SetValuePhase SetValueValidation::ParseValuePhase(std::string_view line)
    {
        SetValuePhase phase;
        const std::vector<std::string> tokens = TokenizeLine(line);
        if (tokens.empty() || tokens.front() != "set")
        {
            return phase;
        }

        if (!IsSetValuePhase(tokens, EndsWithPartialToken(line)))
        {
            return phase;
        }

        phase.bActive = true;
        phase.PropertyPathText = tokens[1];
        phase.ValuePrefix = ExtractSetValuePrefix(tokens, EndsWithPartialToken(line), CurrentToken(line));
        return phase;
    }

    bool SetValueValidation::TryGetValueInfo(
        const CommandContext& context,
        std::string_view propertyPathText,
        PropertySetValueInfo& outInfo)
    {
        outInfo = {};

        const std::optional<PropertyPath> propertyPath = PropertyPath::Parse(propertyPathText);
        if (!propertyPath.has_value())
        {
            return false;
        }

        const Reflection::MEProperty* leafProperty = nullptr;
        if (!propertyPath->TryResolveLeafProperty(context, leafProperty) || leafProperty == nullptr)
        {
            return false;
        }

        outInfo.bWritable = PropertyPath::IsPropertyWritable(*leafProperty);
        if (leafProperty->GetCategory() != Reflection::MEPropertyCategory::Primitive)
        {
            return false;
        }

        const Reflection::MEPrimitiveProperty* primitiveProperty =
            static_cast<const Reflection::MEPrimitiveProperty*>(leafProperty);
        outInfo.Kind = ClassifyPrimitiveProperty(*primitiveProperty);
        if (outInfo.Kind == PropertySetValueKind::Enum)
        {
            outInfo.EnumType = primitiveProperty->GetEnum();
        }

        return outInfo.Kind != PropertySetValueKind::Unknown;
    }

    PropertyValueValidation SetValueValidation::ValidateValuePrefix(
        const PropertySetValueInfo& info,
        std::string_view valuePrefix)
    {
        PropertyValueValidation validation;
        validation.ValueInfo = info;

        if (valuePrefix.empty())
        {
            return validation;
        }

        if (!info.bWritable)
        {
            validation.State = PropertyValueValidationState::Invalid;
            validation.Message = "read-only property";
            return validation;
        }

        switch (info.Kind)
        {
            case PropertySetValueKind::Bool:
            {
                bool boolValue = false;
                if (IsBoolLiteral(valuePrefix, boolValue))
                {
                    validation.State = PropertyValueValidationState::Valid;
                    return validation;
                }

                validation.State = IsBoolLiteralPrefix(valuePrefix) ? PropertyValueValidationState::Partial
                                                                  : PropertyValueValidationState::Invalid;
                if (validation.State == PropertyValueValidationState::Invalid)
                {
                    validation.Message = "expected bool (true/false)";
                }
                return validation;
            }
            case PropertySetValueKind::Enum:
            {
                if (info.EnumType == nullptr)
                {
                    validation.State = PropertyValueValidationState::Invalid;
                    validation.Message = "unresolved enum type";
                    return validation;
                }

                if (info.EnumType->FindByName(std::string(valuePrefix)) != nullptr)
                {
                    validation.State = PropertyValueValidationState::Valid;
                    return validation;
                }

                for (const Reflection::MEEnumEntry& entry : info.EnumType->GetEntries())
                {
                    if (StartsWithIgnoreCase(entry.name, valuePrefix))
                    {
                        validation.State = PropertyValueValidationState::Partial;
                        return validation;
                    }
                }

                validation.State = PropertyValueValidationState::Invalid;
                validation.Message = "expected enum value";
                return validation;
            }
            case PropertySetValueKind::SignedInteger:
            {
                int64_t parsedValue = 0;
                if (TryParseSignedInteger(valuePrefix, parsedValue))
                {
                    validation.State = PropertyValueValidationState::Valid;
                    return validation;
                }

                validation.State = IsNumericPrefix(valuePrefix) ? PropertyValueValidationState::Partial
                                                              : PropertyValueValidationState::Invalid;
                if (validation.State == PropertyValueValidationState::Invalid)
                {
                    validation.Message = "expected integer";
                }
                return validation;
            }
            case PropertySetValueKind::UnsignedInteger:
            {
                uint64_t parsedValue = 0;
                if (TryParseUnsignedInteger(valuePrefix, parsedValue))
                {
                    validation.State = PropertyValueValidationState::Valid;
                    return validation;
                }

                validation.State = IsNumericPrefix(valuePrefix) ? PropertyValueValidationState::Partial
                                                              : PropertyValueValidationState::Invalid;
                if (validation.State == PropertyValueValidationState::Invalid)
                {
                    validation.Message = "expected unsigned integer";
                }
                return validation;
            }
            case PropertySetValueKind::Float:
            case PropertySetValueKind::Double:
            {
                double parsedValue = 0.0;
                if (TryParseFloat(valuePrefix, parsedValue) && !IsIncompleteNumericLiteral(valuePrefix))
                {
                    validation.State = PropertyValueValidationState::Valid;
                    return validation;
                }

                validation.State = IsNumericPrefix(valuePrefix) ? PropertyValueValidationState::Partial
                                                              : PropertyValueValidationState::Invalid;
                if (validation.State == PropertyValueValidationState::Invalid)
                {
                    validation.Message = "expected number";
                }
                return validation;
            }
            case PropertySetValueKind::String:
                validation.State = PropertyValueValidationState::Partial;
                return validation;
            default:
                validation.State = PropertyValueValidationState::Invalid;
                validation.Message = "unsupported property type";
                return validation;
        }
    }

    PropertyValueValidation SetValueValidation::ValidateInputLine(
        const CommandContext& context,
        std::string_view line)
    {
        const SetValuePhase phase = ParseValuePhase(line);
        if (!phase.bActive || phase.ValuePrefix.empty())
        {
            return {};
        }

        PropertySetValueInfo valueInfo;
        if (!TryGetValueInfo(context, phase.PropertyPathText, valueInfo))
        {
            PropertyValueValidation validation;
            validation.State = PropertyValueValidationState::Invalid;
            validation.Message = "unknown property path";
            return validation;
        }

        return ValidateValuePrefix(valueInfo, phase.ValuePrefix);
    }

    std::vector<CompletionItem> SetValueValidation::CompleteValue(
        const CommandContext& context,
        const SetValuePhase& phase)
    {
        std::vector<CompletionItem> items;
        if (!phase.bActive)
        {
            return items;
        }

        PropertySetValueInfo valueInfo;
        if (!TryGetValueInfo(context, phase.PropertyPathText, valueInfo) || !valueInfo.bWritable)
        {
            return items;
        }

        switch (valueInfo.Kind)
        {
            case PropertySetValueKind::Bool:
                AppendBoolCompletions(phase.ValuePrefix, items);
                break;
            case PropertySetValueKind::Enum:
                if (valueInfo.EnumType != nullptr)
                {
                    AppendEnumCompletions(*valueInfo.EnumType, phase.ValuePrefix, items);
                }
                break;
            default:
                break;
        }

        return items;
    }
}

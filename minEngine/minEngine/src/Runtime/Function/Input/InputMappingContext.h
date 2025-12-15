#pragma once
#include "Core.h"

namespace minEngine
{
    struct InputKey;
    class InputAction;
    class InputModifier;

    struct InputActionKeyMapping
    {
        InputAction* Action;
        InputKey Key;

        // Triggers specific to this mapping
        std::vector<std::shared_ptr<InputTrigger>> Triggers;

        // Modifiers specific to this mapping
        std::vector<std::shared_ptr<InputModifier>> Modifiers;

        InputActionKeyMapping() = default;
        InputActionKeyMapping(InputAction* action, InputKey key)
            : Action(action), Key(key)
        {}
        InputActionKeyMapping(InputAction* action, InputKey key, std::initializer_list<std::shared_ptr<InputModifier>> modifiers)
            : Action(action), Key(key), Modifiers(modifiers)
        {}

        const std::vector<std::shared_ptr<InputTrigger>>& GetTriggers() const { return Triggers; }
        void AddTrigger(std::shared_ptr<InputTrigger> trigger)
        {
            Triggers.push_back(trigger);
        }

        const std::vector<std::shared_ptr<InputModifier>>& GetModifiers() const { return Modifiers; }
        void AddModifier(std::shared_ptr<InputModifier> modifier)
        {
            Modifiers.push_back(modifier);
        }

        bool operator==(const InputActionKeyMapping& other) const
        {
            return (Action == other.Action) && (Key == other.Key);
        }
    };

    class InputMappingContext
    {
    public:
        InputMappingContext() = default;
        InputMappingContext(std::initializer_list<InputActionKeyMapping> mappings)
            : m_Mappings(mappings)
        {}
        ~InputMappingContext() = default;

        void AddMapping(const InputActionKeyMapping& mapping)
        {
            m_Mappings.push_back(mapping);
        }

        void RemoveMapping(const InputActionKeyMapping& mapping)
        {
            m_Mappings.erase(std::remove(m_Mappings.begin(), m_Mappings.end(), mapping), m_Mappings.end());
        }

        const std::vector<InputActionKeyMapping>& GetMappings() const
        {
            return m_Mappings;
        }

    private:
        std::vector<InputActionKeyMapping> m_Mappings;
        
    };
} 
#pragma once
#include "Core.h"
#include "MaterialIR.h"

namespace minEngine
{
    class MIRGraph
    {
    public:
        template<typename TValue, typename... TArgs>
        TValue* CreateValue(TArgs&&... args)
        {
            auto ownedValue = std::make_unique<TValue>(std::forward<TArgs>(args)...);
            TValue* valuePtr = ownedValue.get();
            m_ValueOwners.push_back(std::move(ownedValue));
            m_Values.push_back(valuePtr);
            return valuePtr;
        }

        MIRBlock* GetRootBlock(ShaderStage stage) const
        {
            return m_RootBlocks[stage];
        }

        MIRBlock* GetOrCreateRootBlock(ShaderStage stage)
        {
            if (m_RootBlocks[stage] == nullptr)
            {
                auto ownedBlock = std::make_unique<MIRBlock>();
                ownedBlock->Parent = nullptr;
                ownedBlock->FirstInstruction = nullptr;
                m_RootBlockOwners.push_back(std::move(ownedBlock));
                m_RootBlocks[stage] = m_RootBlockOwners.back().get();
            }
            return m_RootBlocks[stage];
        }

        MIRBlock* CreateBlock(MIRBlock* parent);

        void AddOutput(ShaderStage stage, SetMaterialOutput* output)
        {
            if (output)
            {
                m_Outputs[stage].push_back(output);
            }
        }

        const std::vector<SetMaterialOutput*>& GetOutputs(ShaderStage stage) const
        {
            return m_Outputs[stage];
        }

        std::vector<SetMaterialOutput*>& GetOutputs(ShaderStage stage)
        {
            return m_Outputs[stage];
        }

        const std::vector<MIRValue*>& GetValues() const
        {
            return m_Values;
        }

        void AddDiagnostic(const std::string& diagnostic)
        {
            m_Diagnostics.push_back(diagnostic);
        }

        const std::vector<std::string>& GetDiagnostics() const
        {
            return m_Diagnostics;
        }

        bool IsValid() const
        {
            return m_Diagnostics.empty();
        }

        void Reset()
        {
            m_ValueOwners.clear();
            m_Values.clear();
            for (int i = 0; i < NumStages; ++i)
            {
                m_Outputs[i].clear();
                m_RootBlocks[i] = nullptr;
            }
            m_RootBlockOwners.clear();
            m_BlockOwners.clear();
            m_Diagnostics.clear();
        }

    private:
        std::vector<std::unique_ptr<MIRValue>> m_ValueOwners;
        std::vector<MIRValue*> m_Values;
        std::vector<SetMaterialOutput*> m_Outputs[NumStages];
        std::vector<std::unique_ptr<MIRBlock>> m_RootBlockOwners;
        std::vector<std::unique_ptr<MIRBlock>> m_BlockOwners;
        MIRBlock* m_RootBlocks[NumStages] = { nullptr };
        std::vector<std::string> m_Diagnostics;
    };

}

#include "MIRBuilder.h"
#include "../MaterialEdGraph.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "../MaterialPropertyUtil.h"
#include "MaterialIR.h"
#include "MIREmitter.h"

namespace minEngine
{
    MIRBuilder::MIRBuilder() = default;

    void MIRBuilder::AddRootNodeDef(MaterialGraphNodeDef* nodeDef)
    {
        if (nodeDef != nullptr)
        {
            m_RootNodeDefs.push_back(nodeDef);
        }
    }

    void MIRBuilder::Step_Initialize()
    {
        m_BuildContext.BuiltDefs.clear();
        m_BuildContext.NodeDefStack.clear();
        m_BuildContext.InputValues.clear();
        m_BuildContext.OutputValues.clear();

        for (int propertyIndex = 0; propertyIndex < MaterialPropCount; ++propertyIndex)
        {
            m_AttributeOutputs[propertyIndex] = nullptr;
        }
    }

    void MIRBuilder::Step_GenerateOutputInstructions()
    {
        for (int propertyIndex = 0; propertyIndex < MaterialShadingPropertyCount; ++propertyIndex)
        {
            PrepareSingleMaterialAttribute(static_cast<MaterialProperty>(propertyIndex));
        }

        for (MaterialGraphNodeDef* rootDef : m_RootNodeDefs)
        {
            if (rootDef)
            {
                m_BuildContext.NodeDefStack.push_back(rootDef);
            }
        }
    }

    void MIRBuilder::PrepareSingleMaterialAttribute(MaterialProperty property)
    {
        if (m_Emitter == nullptr || property < 0 || property >= MaterialPropCount)
        {
            return;
        }

        SetMaterialOutput* output = m_Emitter->SetMaterialOutput(property, nullptr);
        m_AttributeOutputs[property] = output;
    }

    MIRValue* MIRBuilder::FetchFlowValueForMaterialProperty(MaterialProperty property)
    {
        if (m_Graph == nullptr)
        {
            return nullptr;
        }

        MaterialPropertyInputDescription description;
        if (!m_Graph->ResolveMaterialPropertyInput(property, description) || description.GraphInput == nullptr)
        {
            return nullptr;
        }

        return m_BuildContext.GetInputValue(description.GraphInput);
    }

    void MIRBuilder::FlowValueIntoMaterialOutput(MaterialProperty property, MIRValue* value)
    {
        if (property < 0 || property >= MaterialPropCount || m_Emitter == nullptr)
        {
            return;
        }

        SetMaterialOutput* output = m_AttributeOutputs[property];
        if (output == nullptr)
        {
            output = m_Emitter->SetMaterialOutput(property, value);
            m_AttributeOutputs[property] = output;
            return;
        }

        if (output->Arg != nullptr)
        {
            return;
        }

        if (value == nullptr)
        {
            value = m_Emitter->ConstantDefaultForProperty(property);
        }

        output->Arg = value;
        output->Type = value->Type;
    }

    void MIRBuilder::Step_FlowValuesIntoMaterialOutputs()
    {
        if (m_Emitter == nullptr)
        {
            return;
        }

        for (int propertyIndex = 0; propertyIndex < MaterialShadingPropertyCount; ++propertyIndex)
        {
            const MaterialProperty property = static_cast<MaterialProperty>(propertyIndex);
            FlowValueIntoMaterialOutput(property, FetchFlowValueForMaterialProperty(property));
        }
    }

    void MIRBuilder::Step_BuildNodeDefsToIRGraph()
    {
        while (!m_BuildContext.NodeDefStack.empty())
        {
            BuildTopNodeDef();
        }
    }

    void MIRBuilder::BuildTopNodeDef()
    {
        m_Emitter->m_CurrentNodeDef = m_BuildContext.NodeDefStack.back();

        if (m_BuildContext.BuiltDefs.count(m_Emitter->m_CurrentNodeDef) > 0)
        {
            // This nodedef has already been built, skip it
            m_BuildContext.NodeDefStack.pop_back();
            return;
        }

        // Push the dependencies of this nodedef that have not been built into the stack
        // TODO: maybe we can use iterators to avoid the copy of the inputs, but currently we just copy them for simplicity
        for (MaterialGraphNodeDefInput& input : m_Emitter->m_CurrentNodeDef->GetInputs())
        {
            if (input.IsConnected() && m_BuildContext.BuiltDefs.count(input.NodeDef) == 0)
            {
                m_BuildContext.NodeDefStack.push_back(input.NodeDef);
            }
        }

        // If some dependencies have not been built, we will build them first in the next iterations, so we can just return here
        if (m_BuildContext.NodeDefStack.back() != m_Emitter->m_CurrentNodeDef)
        {
            return;
        }

        // All dependencies have been built, we can build this nodedef now
        // Take the nodedef out of the stack and mark it as built
        m_BuildContext.NodeDefStack.pop_back();
        m_BuildContext.BuiltDefs.insert(m_Emitter->m_CurrentNodeDef);

        // Flow the value into the nodedef's inputs from their connectec outputs.
        for (MaterialGraphNodeDefInput& input : m_Emitter->m_CurrentNodeDef->GetInputs())
        {
            MaterialGraphNodeDefOutput* connectedOutput = input.GetConnectedOutput();
            if (connectedOutput)
            {
                MIRValue* value = m_BuildContext.GetOutputValue(connectedOutput);
                if (value)
                {
                    m_BuildContext.SetInputValue(&input, value);
                }
            }
        }

        m_Emitter->m_CurrentNodeDef->BuildIR(*m_Emitter);
    }

    void MIRBuilder::Step_AnalyzeIRGraph()
    {
        MIRGraph* graph = m_Emitter->m_Graph;
        if (graph == nullptr)
        {
            return;
        }

        for (int stageIndex = 0; stageIndex < NumStages; ++stageIndex)
        {
            for (MIRValue* value : graph->GetValues())
            {
                if (!value->IsInstructionValue())
                {
                    continue;
                }

                MIRInstruction* instr = static_cast<MIRInstruction*>(value);
                for (MIRValue* use : instr->GetUsesForStage(static_cast<ShaderStage>(stageIndex)))
                {
                    if (MIRInstruction* useInstr = AsInstruction(use))
                    {
                        useInstr->NumUsers[stageIndex] += 1;
                    }
                }
            }
        }
    }

    void MIRBuilder::Step_LinkInstructions()
    {
        MIRGraph* graph = m_Emitter->m_Graph;
        if (graph == nullptr)
        {
            return;
        }

        std::vector<MIRInstruction*> instructionStack;

        for (uint32_t stageIndex = 0; stageIndex < NumStages; ++stageIndex)
        {
            const ShaderStage stage = static_cast<ShaderStage>(stageIndex);
            MIRBlock* rootBlock = graph->GetOrCreateRootBlock(stage);
            instructionStack.clear();

            for (SetMaterialOutput* output : graph->GetOutputs(stage))
            {
                output->Block[stageIndex] = rootBlock;
                instructionStack.push_back(output);
            }

            while (!instructionStack.empty())
            {
                MIRInstruction* instr = instructionStack.back();
                instructionStack.pop_back();

                instr->Next[stageIndex] = instr->Block[stageIndex]->FirstInstruction;
                instr->Block[stageIndex]->FirstInstruction = instr;

                const std::vector<MIRValue*> uses = instr->GetUsesForStage(stage);
                for (int32_t useIndex = 0; useIndex < static_cast<int32_t>(uses.size()); ++useIndex)
                {
                    MIRValue* use = uses[useIndex];
                    MIRInstruction* useInstr = AsInstruction(use);
                    if (useInstr == nullptr)
                    {
                        continue;
                    }

                    MIRBlock* targetBlock = instr->GetDesiredBlockForUse(stage, useIndex);
                    if (targetBlock != instr->Block[stageIndex])
                    {
                        targetBlock->Parent = instr->Block[stageIndex];
                        targetBlock->Level = instr->Block[stageIndex]->Level + 1;
                    }

                    useInstr->Block[stageIndex] = useInstr->Block[stageIndex]
                        ? useInstr->Block[stageIndex]->FindCommonParentWith(targetBlock)
                        : targetBlock;

                    ++useInstr->NumProcessedUsers[stageIndex];
                    if (useInstr->NumProcessedUsers[stageIndex] == useInstr->NumUsers[stageIndex])
                    {
                        instructionStack.push_back(useInstr);
                    }
                }
            }
        }
    }

    void MIRBuilder::Step_Finalize()
    {
        // UE Step_Finalize: module-level compilation metadata (e.g. UV scalar counts).
        // Attribute value wiring belongs in Step_FlowValuesIntoMaterialOutputs.
    }

    void MIRBuilder::Build(const MaterialEdGraph& graph, MIRGraph& targetGraph)
    {
        MIREmitter emitter;
        emitter.m_Builder = this;
        emitter.m_Graph = &targetGraph;

        m_Graph = &graph;
        m_Emitter = &emitter;

        Step_Initialize();
        Step_GenerateOutputInstructions();
        Step_BuildNodeDefsToIRGraph();
        Step_FlowValuesIntoMaterialOutputs();
        Step_AnalyzeIRGraph();
        Step_LinkInstructions();
        Step_Finalize();
    }
}

#include "MaterialIRTest.h"

#include "MIRBuilder.h"
#include "../MaterialCompiler/GLSLMaterialCompiler.h"
#include "../MaterialEdGraph.h"
#include "../MaterialEdGraphNode.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef_Add.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef_Const.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef_Multiply.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef_Output.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef_Texture.h"

#include <memory>
#include <sstream>
#include <vector>

namespace minEngine
{
	namespace
	{
		struct TestGraphBundle
		{
			MaterialEdGraph Graph;
			std::vector<std::unique_ptr<MaterialGraphNodeDef>> NodeDefs;
			int32_t RootNodeIndex = -1;
		};

		const char* ToString(MaterialOp op)
		{
			switch (op)
			{
			case MaterialOp::Constant: return "Constant";
			case MaterialOp::Constant2: return "Constant2";
			case MaterialOp::Constant3: return "Constant3";
			case MaterialOp::Constant4: return "Constant4";
			case MaterialOp::Add: return "Add";
			case MaterialOp::Multiply: return "Multiply";
			case MaterialOp::TextureObject: return "TextureObject";
			case MaterialOp::TextureSample: return "TextureSample";
			default: return "Unknown";
			}
		}

		const char* ToString(MaterialValueType type)
		{
			switch (type)
			{
			case MaterialValueType::Float: return "Float";
			case MaterialValueType::Vector2: return "Vector2";
			case MaterialValueType::Vector3: return "Vector3";
			case MaterialValueType::Vector4: return "Vector4";
			case MaterialValueType::Texture2D: return "Texture2D";
			case MaterialValueType::TextureCube: return "TextureCube";
			default: return "Unknown";
			}
		}

		std::string DumpLiteral(const MaterialLiteralValue& value)
		{
			std::ostringstream stream;
			switch (value.Type)
			{
			case MaterialValueType::Float:
				stream << value.Data[0];
				break;
			case MaterialValueType::Vector2:
				stream << "(" << value.Data[0] << ", " << value.Data[1] << ")";
				break;
			case MaterialValueType::Vector3:
				stream << "(" << value.Data[0] << ", " << value.Data[1] << ", " << value.Data[2] << ")";
				break;
			case MaterialValueType::Vector4:
				stream << "(" << value.Data[0] << ", " << value.Data[1] << ", " << value.Data[2] << ", " << value.Data[3] << ")";
				break;
			default:
				stream << "n/a";
				break;
			}
			return stream.str();
		}

		std::string DumpMIRGraph(const MIRGraph& graph, const MIRValue* outputValue)
		{
			std::ostringstream stream;
			stream << "MIR Values:" << "\n";
			for (const std::unique_ptr<MIRValue>& value : graph.Values)
			{
				stream << "  v" << value->Id << " : " << ToString(value->ValueType);
				if (value->Producer == nullptr)
				{
					stream << " = " << DumpLiteral(value->LiteralValue);
				}
				stream << "\n";
			}

			stream << "MIR Nodes:" << "\n";
			for (const std::unique_ptr<MIRNode>& node : graph.Nodes)
			{
				stream << "  n" << node->Id << " " << ToString(node->Op);
				if (!node->SymbolName.empty())
				{
					stream << " [" << node->SymbolName << "]";
				}

				stream << " (";
				for (size_t i = 0; i < node->Inputs.size(); ++i)
				{
					if (i > 0)
					{
						stream << ", ";
					}
					stream << "v" << node->Inputs[i]->Id;
				}
				stream << ") -> ";

				for (size_t i = 0; i < node->Outputs.size(); ++i)
				{
					if (i > 0)
					{
						stream << ", ";
					}
					stream << "v" << node->Outputs[i]->Id;
				}
				stream << "\n";
			}

			stream << "Output Value: ";
			if (outputValue)
			{
				stream << "v" << outputValue->Id;
			}
			else
			{
				stream << "<null>";
			}
			stream << "\n";
			return stream.str();
		}

		TestGraphBundle BuildMvpGraph()
		{
			TestGraphBundle bundle;

			auto addNode = [&](std::unique_ptr<MaterialGraphNodeDef> def) -> int32_t
			{
				bundle.NodeDefs.push_back(std::move(def));
				MaterialEdGraphNode node;
				node.m_Definition = bundle.NodeDefs.back().get();
				bundle.Graph.m_Nodes.push_back(node);
				return static_cast<int32_t>(bundle.Graph.m_Nodes.size() - 1);
			};

			const int32_t c1 = addNode(std::make_unique<MaterialGraphNodeDef_Constant>(1.0f));
			const int32_t c2 = addNode(std::make_unique<MaterialGraphNodeDef_Constant>(2.0f));
			const int32_t add1 = addNode(std::make_unique<MaterialGraphNodeDef_Add>());
			const int32_t c3 = addNode(std::make_unique<MaterialGraphNodeDef_Constant>(3.0f));
			const int32_t mul1 = addNode(std::make_unique<MaterialGraphNodeDef_Multiply>());
			const int32_t c0 = addNode(std::make_unique<MaterialGraphNodeDef_Constant>(0.0f));
			const int32_t deadMul = addNode(std::make_unique<MaterialGraphNodeDef_Multiply>());
			const int32_t add2 = addNode(std::make_unique<MaterialGraphNodeDef_Add>());
			const int32_t uv = addNode(std::make_unique<MaterialGraphNodeDef_Constant2>(0.25f, 0.75f));
			const int32_t texParam = addNode(std::make_unique<MaterialGraphNodeDef_Texture2DParameter>("u_TestTexture"));
			const int32_t sample = addNode(std::make_unique<MaterialGraphNodeDef_TextureSample>());
			const int32_t finalAdd = addNode(std::make_unique<MaterialGraphNodeDef_Add>());
			const int32_t materialOutput = addNode(std::make_unique<MaterialGraphNodeDef_MaterialOutput>());

			bundle.Graph.ConnectNodes(c1, 0, add1, 0);
			bundle.Graph.ConnectNodes(c2, 0, add1, 1);
			bundle.Graph.ConnectNodes(add1, 0, mul1, 0);
			bundle.Graph.ConnectNodes(c3, 0, mul1, 1);

			bundle.Graph.ConnectNodes(c3, 0, deadMul, 0);
			bundle.Graph.ConnectNodes(c0, 0, deadMul, 1);

			bundle.Graph.ConnectNodes(mul1, 0, add2, 0);
			bundle.Graph.ConnectNodes(deadMul, 0, add2, 1);

			bundle.Graph.ConnectNodes(texParam, 0, sample, 0);
			bundle.Graph.ConnectNodes(uv, 0, sample, 1);

			bundle.Graph.ConnectNodes(sample, 0, finalAdd, 0);
			bundle.Graph.ConnectNodes(add2, 0, finalAdd, 1);
			bundle.Graph.ConnectNodes(finalAdd, 0, materialOutput, 0);

			bundle.RootNodeIndex = materialOutput;
			return bundle;
		}
	}

	std::string BuildMaterialMvpIRDump()
	{
		TestGraphBundle bundle = BuildMvpGraph();

		MIRGraph irGraph;
		MIRBuilder builder(irGraph);
		MIRValue* outputValue = builder.BuildNodeOutput(*bundle.Graph.m_Nodes[bundle.RootNodeIndex].m_Definition, 0);

		return DumpMIRGraph(irGraph, outputValue);
	}

	std::string BuildMaterialMvpGLSLSource()
	{
		TestGraphBundle bundle = BuildMvpGraph();

		GLSLMaterialCompiler compiler;
		MaterialCompileResult result = compiler.Compile(bundle.Graph);

		if (result.IsSuccess())
		{
			return result.ShaderSource;
		}

		std::ostringstream stream;
		stream << "Material compile failed:" << "\n";
		for (const MaterialCompileDiagnostic& diagnostic : result.Diagnostics)
		{
			stream << "  - " << diagnostic.Message << "\n";
		}
		return stream.str();
	}
}
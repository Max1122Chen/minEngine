#pragma once
#include "Core.h"
#include "MaterialIR.h"

namespace minEngine
{
    class MIRGraph;

    // UE-style textual MIR dump (see Unreal MaterialIRDebug.cpp).
    std::string DebugDumpMIR(const MIRGraph& graph, const char* materialName = "Material");

    // Writes dump to disk when parent directories exist; returns false on I/O failure.
    bool WriteMIRDumpToFile(const MIRGraph& graph, const std::string& filePath, const char* materialName = "Material");
}

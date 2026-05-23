#include "MaterialTestGraph.h"

#include "../Material.h"
#include "MaterialCompiler/MaterialCompileTypes.h"

#include <fstream>
#include <sstream>

namespace minEngine
{
    bool VerifyGoldenMaterialIRSmokeMemtlOnDisk(std::string* outError)
    {
        const std::filesystem::path assetPath = ResolveGoldenMaterialIRSmokeMemtlPath();
        if (assetPath.empty())
        {
            if (outError != nullptr)
            {
                *outError = "Golden MaterialIRSmoke.memtl path not found.";
            }
            return false;
        }

        std::ifstream input(assetPath);
        if (!input.is_open())
        {
            if (outError != nullptr)
            {
                *outError = "Failed to open golden MaterialIRSmoke.memtl.";
            }
            return false;
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        const std::string contents = buffer.str();

        auto fail = [&](const char* message)
        {
            if (outError != nullptr)
            {
                *outError = message;
            }
            return false;
        };

        if (contents.find("\"m_ShadingModel\": 1") == std::string::npos)
        {
            return fail("Golden asset missing '\"m_ShadingModel\": 1' (BlinnPhong).");
        }

        if (contents.find("\"m_BlendMode\": 0") == std::string::npos)
        {
            return fail("Golden asset missing '\"m_BlendMode\": 0' (Opaque).");
        }

        if (contents.find("\"m_Name\": \"MaterialIRSmoke\"") == std::string::npos)
        {
            return fail("Golden asset missing MaterialIRSmoke name field.");
        }

        return true;
    }
}

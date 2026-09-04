#include "Runtime/Resource/Loaders/AssimpMeshImportUtil.h"

#include "assimp/Exporter.hpp"
#include "assimp/Importer.hpp"
#include "assimp/matrix4x4.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include <cmath>
#include <filesystem>
#include <system_error>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace minEngine
{
    unsigned int AssimpMeshImportUtil::GetDefaultPostProcessFlags()
    {
        return aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals;
    }

    Vector4 AssimpMeshImportUtil::ComputeFallbackTangent(const Vector3& normal)
    {
        const Vector3 up =
            (std::abs(normal.y) < 0.999f) ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
        Vector3 tangent = glm::normalize(glm::cross(up, normal));
        return Vector4(tangent, 1.0f);
    }

    Vector4 AssimpMeshImportUtil::ComputeTangentWithHandedness(
        const Vector3& normal,
        const Vector3* tangent,
        const Vector3* bitangent)
    {
        if (tangent == nullptr)
        {
            return ComputeFallbackTangent(normal);
        }

        Vector3 tangentValue = *tangent;
        if (glm::dot(tangentValue, tangentValue) <= 1e-12f)
        {
            return ComputeFallbackTangent(normal);
        }

        tangentValue = glm::normalize(tangentValue - normal * glm::dot(normal, tangentValue));
        float handedness = 1.0f;
        if (bitangent != nullptr)
        {
            handedness =
                (glm::dot(glm::cross(normal, tangentValue), *bitangent) < 0.0f) ? -1.0f : 1.0f;
        }

        return Vector4(tangentValue, handedness);
    }

    Matrix4 AssimpMeshImportUtil::ConvertMatrix(const aiMatrix4x4& matrix)
    {
        // Assimp row-major → glm column-major.
        return Matrix4(
            matrix.a1,
            matrix.b1,
            matrix.c1,
            matrix.d1,
            matrix.a2,
            matrix.b2,
            matrix.c2,
            matrix.d2,
            matrix.a3,
            matrix.b3,
            matrix.c3,
            matrix.d3,
            matrix.a4,
            matrix.b4,
            matrix.c4,
            matrix.d4);
    }

    void AssimpMeshImportUtil::DecomposeMatrix(const Matrix4& matrix, Transform& outTransform)
    {
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        if (!glm::decompose(matrix, scale, rotation, translation, skew, perspective))
        {
            outTransform = Transform{};
            return;
        }

        outTransform.Position = translation;
        outTransform.SetRotation(Quaternion::FromGlm(glm::normalize(rotation)));
        outTransform.Scale = scale;
    }

    bool AssimpMeshImportUtil::CookExternalMeshToFile(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& destPath,
        const char* exportFormatId,
        std::string* outError)
    {
        auto setError = [outError](const std::string& message)
        {
            if (outError != nullptr)
            {
                *outError = message;
            }
        };

        if (!std::filesystem::exists(sourcePath) || !std::filesystem::is_regular_file(sourcePath))
        {
            setError("source file does not exist: " + sourcePath.string());
            return false;
        }

        std::error_code createError;
        std::filesystem::create_directories(destPath.parent_path(), createError);
        if (createError)
        {
            setError("failed to create destination directory: " + createError.message());
            return false;
        }

        const bool useCopyOnly =
            exportFormatId == nullptr || exportFormatId[0] == '\0'
            || sourcePath.extension() == destPath.extension();

        if (useCopyOnly)
        {
            std::error_code copyError;
            std::filesystem::copy_file(
                sourcePath,
                destPath,
                std::filesystem::copy_options::overwrite_existing,
                copyError);
            if (copyError)
            {
                setError("copy failed: " + copyError.message());
                return false;
            }
            return true;
        }

        Assimp::Importer importer;
        const aiScene* scene =
            importer.ReadFile(sourcePath.string().c_str(), GetDefaultPostProcessFlags());
        if (scene == nullptr)
        {
            setError(std::string("Assimp import failed: ") + importer.GetErrorString());
            return false;
        }

        Assimp::Exporter exporter;
        const aiReturn exportResult =
            exporter.Export(scene, exportFormatId, destPath.string().c_str());
        if (exportResult != aiReturn_SUCCESS)
        {
            setError(std::string("Assimp export failed: ") + exporter.GetErrorString());
            return false;
        }

        return true;
    }
}

#pragma once
#include "Core.h"
#include "Core/Math/Math.h"
#include "Runtime/Core/Object/MEObject.h"

#include <string>

namespace minEngine
{
    // TODO: we should refactor this to be a more modern shader system.
    ME_CLASS()
    class RHIShader : public MEObject
    {
        ME_GENERATED_BODY(RHIShader)
    public:
        RHIShader() = default;
        virtual ~RHIShader() = default;

        bool IsValid() const { return m_IsValid; }
        const std::string& GetCompileLog() const { return m_CompileLog; }

        virtual void Use() = 0;
        virtual void UploadUniformInt(const std::string& name, int value) = 0;
        virtual void UploadUniformFloat(const std::string& name, float value) = 0;
        virtual void UploadUniformFloat2(const std::string& name, const Vector2& value) = 0;
        virtual void UploadUniformFloat3(const std::string& name, const Vector3& value) = 0;
        virtual void UploadUniformFloat4(const std::string& name, const Vector4& value) = 0;
        virtual void UploadUniformMat4(const std::string& name, const float* matrix) = 0;
        virtual void UploadUniformMat4(const std::string& name, const Matrix4& matrix) = 0;

        virtual void BindUniformBlock(const std::string& blockName, uint32_t bindingPoint) = 0;

    protected:
        bool m_IsValid = false;
        std::string m_CompileLog;
    };
} 

#include "RHIShader.gen.h"


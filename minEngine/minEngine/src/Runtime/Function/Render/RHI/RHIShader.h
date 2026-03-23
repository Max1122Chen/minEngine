#pragma once
#include "Core.h"
#include "Core/Math/Math.h"

namespace minEngine
{
    class RHIShader
    {
    public:
        RHIShader() = default;
        virtual ~RHIShader() = default;

        virtual void Use() = 0;
        virtual void UploadUniformInt(const std::string& name, int value) = 0;
        virtual void UploadUniformFloat(const std::string& name, float value) = 0;
        virtual void UploadUniformFloat3(const std::string& name, Vector3 value) = 0;
        virtual void UploadUniformMat4(const std::string& name, const float* matrix) = 0;
        virtual void UploadUniformMat4(const std::string& name, const Matrix4& matrix) = 0;
    };
} 

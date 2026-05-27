#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "EngineAPI.h"

namespace minEngine::Reflection
{
    class MEFunction;

    class MINENGINE_API MEFunctionFrame
    {
    public:
        explicit MEFunctionFrame(const MEFunction& function);
        ~MEFunctionFrame();

        MEFunctionFrame(const MEFunctionFrame&) = delete;
        MEFunctionFrame& operator=(const MEFunctionFrame&) = delete;

        void* GetBuffer();
        const void* GetBuffer() const;

        const MEFunction& GetFunction() const { return m_Function; }

        bool SetParam(const std::string& name, const void* value, size_t valueSize);
        bool GetParam(const std::string& name, void* outValue, size_t outValueSize) const;

        template<typename T>
        bool SetParam(const std::string& name, const T& value)
        {
            return SetParam(name, &value, sizeof(T));
        }

        template<typename T>
        bool GetParam(const std::string& name, T& outValue) const
        {
            return GetParam(name, &outValue, sizeof(T));
        }

    private:
        const MEFunction& m_Function;
        std::vector<uint8_t> m_Buffer;
    };

} // namespace minEngine::Reflection

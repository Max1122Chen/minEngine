#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "EngineAPI.h"
#include "MEFunction.h"

namespace minEngine::Reflection
{
    class MEProperty;

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

        bool SetParamPtr(const std::string& name, void* ptr);
        bool SetParamConstPtr(const std::string& name, const void* ptr);
        bool GetParamPtr(const std::string& name, void*& outPtr) const;
        bool GetParamConstPtr(const std::string& name, const void*& outPtr) const;

        template<typename TValue>
        bool GetParamValuePtr(const std::string& name, const TValue*& outPtr) const
        {
            const MEParamDescriptor* param = m_Function.FindParam(name);
            if (param == nullptr || param->Property == nullptr)
            {
                return false;
            }

            if (param->Role == MEParamRole::Out || param->PassKind == MEParamPassKind::Ref
                || param->PassKind == MEParamPassKind::ConstRef)
            {
                return false;
            }

            if (param->Property->GetStorageSize() != sizeof(TValue))
            {
                return false;
            }

            const uint8_t* base = m_Buffer.empty() ? nullptr : m_Buffer.data();
            if (base == nullptr)
            {
                return false;
            }

            outPtr = reinterpret_cast<const TValue*>(base + param->Offset);
            return true;
        }

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

        template<typename T>
        bool SetParamRef(const std::string& name, T& value)
        {
            return SetParamPtr(name, static_cast<void*>(&value));
        }

        template<typename T>
        bool SetParamConstRef(const std::string& name, const T& value)
        {
            return SetParamConstPtr(name, static_cast<const void*>(&value));
        }

        template<typename T>
        bool SetOutParam(const std::string& name, T& outValue)
        {
            return SetParamPtr(name, static_cast<void*>(&outValue));
        }

    private:
        struct LifetimeSlot
        {
            const MEProperty* Property = nullptr;
            uint32_t Offset = 0;
        };

        const MEFunction& m_Function;
        std::vector<uint8_t> m_Buffer;
        std::vector<LifetimeSlot> m_LifetimeSlots;
    };

} // namespace minEngine::Reflection

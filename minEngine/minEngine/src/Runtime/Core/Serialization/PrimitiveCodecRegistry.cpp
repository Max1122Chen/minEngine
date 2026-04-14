#include "PrimitiveCodecRegistry.h"

#include "Runtime/Core/Math/Math.h"

#include <initializer_list>
#include <limits>
#include <string>
#include <typeinfo>

namespace minEngine::Serialization
{   

    PrimitiveCodecRegistry& PrimitiveCodecRegistry::Get()
    {
        static PrimitiveCodecRegistry registry;
        registry.RegisterDefaultCodecs();
        return registry;
    }

    void PrimitiveCodecRegistry::Register(const std::string& typeName, PrimitiveCodec codec)
    {
        if (typeName.empty())
        {
            return;
        }

        if (codec.write == nullptr || codec.read == nullptr)
        {
            return;
        }

        m_CodecsByTypeName[typeName] = codec;
    }

    const PrimitiveCodec* PrimitiveCodecRegistry::Find(const std::string& typeName) const
    {
        auto iter = m_CodecsByTypeName.find(typeName);
        if (iter == m_CodecsByTypeName.end())
        {
            return nullptr;
        }

        return &iter->second;
    }

    bool PrimitiveCodecRegistry::Has(const std::string& typeName) const
    {
        return Find(typeName) != nullptr;
    }

    void PrimitiveCodecRegistry::RegisterDefaultCodecs()
    {
        if (m_DefaultCodecsRegistered)
        {
            return;
        }

        RegisterCodecWithAliases(
            PrimitiveCodec{
                [](WriterArchive& archive, const void* valuePtr) -> bool
                {
                    if (valuePtr == nullptr)
                    {
                        return false;
                    }
                    return archive.WriteBool(*static_cast<const bool*>(valuePtr));
                },
                [](ReaderArchive& archive, void* outValuePtr) -> bool
                {
                    if (outValuePtr == nullptr)
                    {
                        return false;
                    }
                    return archive.ReadBool(*static_cast<bool*>(outValuePtr));
                }},
            {"bool", typeid(bool).name()});

        RegisterCodecWithAliases(
            PrimitiveCodec{
                [](WriterArchive& archive, const void* valuePtr) -> bool
                {
                    if (valuePtr == nullptr)
                    {
                        return false;
                    }
                    return archive.WriteInt64(static_cast<int64_t>(*static_cast<const int*>(valuePtr)));
                },
                [](ReaderArchive& archive, void* outValuePtr) -> bool
                {
                    if (outValuePtr == nullptr)
                    {
                        return false;
                    }

                    int64_t value = 0;
                    if (!archive.ReadInt64(value))
                    {
                        return false;
                    }

                    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
                    {
                        return false;
                    }

                    *static_cast<int*>(outValuePtr) = static_cast<int>(value);
                    return true;
                }},
            {"int", typeid(int).name()});

        RegisterCodecWithAliases(
            PrimitiveCodec{
                [](WriterArchive& archive, const void* valuePtr) -> bool
                {
                    if (valuePtr == nullptr)
                    {
                        return false;
                    }
                    return archive.WriteDouble(static_cast<double>(*static_cast<const float*>(valuePtr)));
                },
                [](ReaderArchive& archive, void* outValuePtr) -> bool
                {
                    if (outValuePtr == nullptr)
                    {
                        return false;
                    }

                    double value = 0.0;
                    if (!archive.ReadDouble(value))
                    {
                        return false;
                    }

                    *static_cast<float*>(outValuePtr) = static_cast<float>(value);
                    return true;
                }},
            {"float", typeid(float).name()});

        RegisterCodecWithAliases(
            PrimitiveCodec{
                [](WriterArchive& archive, const void* valuePtr) -> bool
                {
                    if (valuePtr == nullptr)
                    {
                        return false;
                    }
                    return archive.WriteDouble(*static_cast<const double*>(valuePtr));
                },
                [](ReaderArchive& archive, void* outValuePtr) -> bool
                {
                    if (outValuePtr == nullptr)
                    {
                        return false;
                    }
                    return archive.ReadDouble(*static_cast<double*>(outValuePtr));
                }},
            {"double", typeid(double).name()});

        RegisterCodecWithAliases(
            PrimitiveCodec{
                [](WriterArchive& archive, const void* valuePtr) -> bool
                {
                    if (valuePtr == nullptr)
                    {
                        return false;
                    }
                    return archive.WriteString(*static_cast<const std::string*>(valuePtr));
                },
                [](ReaderArchive& archive, void* outValuePtr) -> bool
                {
                    if (outValuePtr == nullptr)
                    {
                        return false;
                    }
                    return archive.ReadString(*static_cast<std::string*>(outValuePtr));
                }},
            {"std::string", typeid(std::string).name()});

        RegisterCodecWithAliases(
            PrimitiveCodec{
                [](WriterArchive& archive, const void* valuePtr) -> bool
                {
                    if (valuePtr == nullptr)
                    {
                        return false;
                    }

                    const Vector2& value = *static_cast<const Vector2*>(valuePtr);
                    return archive.BeginArray(2)
                        && archive.WriteDouble(value.x)
                        && archive.WriteDouble(value.y)
                        && archive.EndArray();
                },
                [](ReaderArchive& archive, void* outValuePtr) -> bool
                {
                    if (outValuePtr == nullptr)
                    {
                        return false;
                    }

                    size_t count = 0;
                    if (!archive.BeginArray(count) || count != 2)
                    {
                        return false;
                    }

                    Vector2& value = *static_cast<Vector2*>(outValuePtr);
                    double x = 0.0;
                    if (!archive.EnterArrayElement(0) || !archive.ReadDouble(x) || !archive.LeaveArrayElement())
                    {
                        return false;
                    }
                    double y = 0.0;
                    if (!archive.EnterArrayElement(1) || !archive.ReadDouble(y) || !archive.LeaveArrayElement())
                    {
                        return false;
                    }

                    value.x = static_cast<float>(x);
                    value.y = static_cast<float>(y);

                    return archive.EndArray();
                }},
            {"Vector2", "minEngine::Vector2", typeid(Vector2).name()});

        RegisterCodecWithAliases(
            PrimitiveCodec{
                [](WriterArchive& archive, const void* valuePtr) -> bool
                {
                    if (valuePtr == nullptr)
                    {
                        return false;
                    }

                    const Vector3& value = *static_cast<const Vector3*>(valuePtr);
                    return archive.BeginArray(3)
                        && archive.WriteDouble(value.x)
                        && archive.WriteDouble(value.y)
                        && archive.WriteDouble(value.z)
                        && archive.EndArray();
                },
                [](ReaderArchive& archive, void* outValuePtr) -> bool
                {
                    if (outValuePtr == nullptr)
                    {
                        return false;
                    }

                    size_t count = 0;
                    if (!archive.BeginArray(count) || count != 3)
                    {
                        return false;
                    }

                    Vector3& value = *static_cast<Vector3*>(outValuePtr);
                    double x = 0.0;
                    if (!archive.EnterArrayElement(0) || !archive.ReadDouble(x) || !archive.LeaveArrayElement())
                    {
                        return false;
                    }
                    double y = 0.0;
                    if (!archive.EnterArrayElement(1) || !archive.ReadDouble(y) || !archive.LeaveArrayElement())
                    {
                        return false;
                    }
                    double z = 0.0;
                    if (!archive.EnterArrayElement(2) || !archive.ReadDouble(z) || !archive.LeaveArrayElement())
                    {
                        return false;
                    }

                    value.x = static_cast<float>(x);
                    value.y = static_cast<float>(y);
                    value.z = static_cast<float>(z);

                    return archive.EndArray();
                }},
            {"Vector3", "minEngine::Vector3", typeid(Vector3).name()});

        RegisterCodecWithAliases(
            PrimitiveCodec{
                [](WriterArchive& archive, const void* valuePtr) -> bool
                {
                    if (valuePtr == nullptr)
                    {
                        return false;
                    }

                    const Vector4& value = *static_cast<const Vector4*>(valuePtr);
                    return archive.BeginArray(4)
                        && archive.WriteDouble(value.x)
                        && archive.WriteDouble(value.y)
                        && archive.WriteDouble(value.z)
                        && archive.WriteDouble(value.w)
                        && archive.EndArray();
                },
                [](ReaderArchive& archive, void* outValuePtr) -> bool
                {
                    if (outValuePtr == nullptr)
                    {
                        return false;
                    }

                    size_t count = 0;
                    if (!archive.BeginArray(count) || count != 4)
                    {
                        return false;
                    }

                    Vector4& value = *static_cast<Vector4*>(outValuePtr);
                    double x = 0.0;
                    if (!archive.EnterArrayElement(0) || !archive.ReadDouble(x) || !archive.LeaveArrayElement())
                    {
                        return false;
                    }
                    double y = 0.0;
                    if (!archive.EnterArrayElement(1) || !archive.ReadDouble(y) || !archive.LeaveArrayElement())
                    {
                        return false;
                    }
                    double z = 0.0;
                    if (!archive.EnterArrayElement(2) || !archive.ReadDouble(z) || !archive.LeaveArrayElement())
                    {
                        return false;
                    }
                    double w = 0.0;
                    if (!archive.EnterArrayElement(3) || !archive.ReadDouble(w) || !archive.LeaveArrayElement())
                    {
                        return false;
                    }

                    value.x = static_cast<float>(x);
                    value.y = static_cast<float>(y);
                    value.z = static_cast<float>(z);
                    value.w = static_cast<float>(w);

                    return archive.EndArray();
                }},
            {"Vector4", "minEngine::Vector4", typeid(Vector4).name()});

        m_DefaultCodecsRegistered = true;
    }

    void PrimitiveCodecRegistry::RegisterCodecWithAliases( PrimitiveCodec codec,
                                   std::initializer_list<std::string> aliases)
    {
        for (const std::string& alias : aliases)
        {
            Register(alias, codec);
        }
    }
}

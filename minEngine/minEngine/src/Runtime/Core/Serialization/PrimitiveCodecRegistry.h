#pragma once

#include "Archive.h"

#include <string>
#include <unordered_map>

namespace minEngine::Serialization
{
    struct PrimitiveCodec
    {
        bool (*write)(WriterArchive& archive, const void* valuePtr) = nullptr;
        bool (*read)(ReaderArchive& archive, void* outValuePtr) = nullptr;
    };

    class PrimitiveCodecRegistry
    {
    public:
        static PrimitiveCodecRegistry& Get();

        void Register(const std::string& typeName, PrimitiveCodec codec);
        const PrimitiveCodec* Find(const std::string& typeName) const;
        bool Has(const std::string& typeName) const;

        void RegisterDefaultCodecs();

        void RegisterCodecWithAliases(PrimitiveCodec codec, std::initializer_list<std::string> aliases);

    private:
        bool m_DefaultCodecsRegistered = false;
        std::unordered_map<std::string, PrimitiveCodec> m_CodecsByTypeName;
    };
}

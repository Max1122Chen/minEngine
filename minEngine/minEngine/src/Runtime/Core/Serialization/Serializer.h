#pragma once

#include "Core.h"
#include "Archive.h"
#include "SerializationTypes.h"

#include <string>

namespace minEngine::Reflection
{
    class MEClass;
    class MEProperty;
    class MEObjectProperty;
    class MEObjectPtrProperty;
}

namespace minEngine::Serialization
{
    class MINENGINE_API Serializer
    {
    public:
        static SerializeResult Serialize(const std::string& rootClassName,
                                         const void* rootObject,
                                         WriterArchive& archive,
                                         const SerializerOptions& options = SerializerOptions{});

        static SerializeResult Deserialize(const std::string& rootClassName,
                                           void* outRootObject,
                                           ReaderArchive& archive,
                                           const SerializerOptions& options = SerializerOptions{});

    private:
        static SerializeResult SerializeObjectInstance(const minEngine::Reflection::MEClass* classInfo,
                                              const void* objectPtr,
                                              WriterArchive& archive,
                                              const SerializerOptions& options,
                                              const std::string& path);

        static SerializeResult SerializeProperty(const minEngine::Reflection::MEProperty& property,
                                                 const void* valuePtr,
                                                 const void* ownerObjectPtr,
                                                 WriterArchive& archive,
                                                 const SerializerOptions& options,
                                                 const std::string& path);

        static SerializeResult SerializeObjectPtr(const minEngine::Reflection::MEObjectPtrProperty& objectPtrProperty,
                                                 const void* ptrToPtr,
                                                 const void* ownerObjectPtr,
                                                 WriterArchive& archive,
                                                 const SerializerOptions& options,
                                                 const std::string& path);

        static bool SerializeObject_IterateProps(const minEngine::Reflection::MEClass* classInfo,
                                        const void* objectPtr,
                                        WriterArchive& archive,
                                        const SerializerOptions& options,
                                        const std::string& path);

        static SerializeResult DeserializeObjectInstance(const minEngine::Reflection::MEClass* classInfo,
                                                void* objectPtr,
                                                ReaderArchive& archive,
                                                const SerializerOptions& options,
                                                const std::string& path);

        static SerializeResult DeserializeProperty(const minEngine::Reflection::MEProperty& property,
                                                   void* outValuePtr,
                                                   ReaderArchive& archive,
                                                   const SerializerOptions& options,
                                                   const std::string& path);

        static SerializeResult DeserializeObjectPtr(const minEngine::Reflection::MEObjectPtrProperty& objectPtrProperty,
                                                   void* ptrToPtr,
                                                   ReaderArchive& archive,
                                                   const SerializerOptions& options,
                                                   const std::string& path);

        static bool DeserializeObject_IterateProps(const minEngine::Reflection::MEClass* classInfo,
                                                void* objectPtr,
                                                ReaderArchive& archive,
                                                const SerializerOptions& options,
                                                const std::string& path);

        static std::string JoinPath(const std::string& basePath, const std::string& nextSegment);

    public:
        
    private:
        static bool IsHandlingPtr()
        {
            return m_IsHandlingPtr;
        }

        static void SetHandlingPtr(bool handling)
        {
            m_IsHandlingPtr = handling;
        }
        static bool m_IsHandlingPtr;
    };
}

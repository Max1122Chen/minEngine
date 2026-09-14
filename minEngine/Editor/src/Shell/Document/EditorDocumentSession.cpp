#include "Shell/Document/EditorDocumentSession.h"

namespace minEngine
{
    EditorDocumentSession::EditorDocumentSession(
        EditorDocumentId id,
        std::string typeId,
        std::string assetKey,
        std::string title)
        : m_Id(id)
        , m_TypeId(std::move(typeId))
        , m_AssetKey(std::move(assetKey))
        , m_Title(std::move(title))
    {
    }
}

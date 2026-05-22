#include "MaterialGraphIds.h"

#include "Runtime/Core/Hash/Hash.h"
#include "Runtime/Function/Render/Material/MaterialEdGraphNode.h"

#include "imgui_node_editor.h"

#include <unordered_map>

namespace minEngine
{
    namespace
    {
        constexpr uintptr_t kPinTypeTag = 1ull;
        constexpr uintptr_t kLinkTypeTag = 3ull;
        constexpr uintptr_t kTypeTagMask = 3ull;

        size_t HashPinKey(const MaterialGraphIds::PinKey& key)
        {
            size_t seed = 0;
            seed = HashCombine(seed, key.Node);
            seed = HashCombine(seed, static_cast<int>(key.Kind));
            seed = HashCombine(seed, key.Index);
            return seed;
        }

        size_t HashLinkKey(const MaterialGraphIds::LinkKey& key)
        {
            size_t seed = 0;
            seed = HashCombine(seed, key.FromNode);
            seed = HashCombine(seed, key.FromOutputIndex);
            seed = HashCombine(seed, key.ToNode);
            seed = HashCombine(seed, key.ToInputIndex);
            return seed;
        }

        struct PinKeyHash
        {
            size_t operator()(const MaterialGraphIds::PinKey& key) const
            {
                return HashPinKey(key);
            }
        };

        struct LinkKeyHash
        {
            size_t operator()(const MaterialGraphIds::LinkKey& key) const
            {
                return HashLinkKey(key);
            }
        };

        uintptr_t EncodePinEditorId(size_t hash)
        {
            uintptr_t id = (static_cast<uintptr_t>(hash) << 2) | kPinTypeTag;
            if (id == 0ull)
            {
                id = kPinTypeTag;
            }
            return id;
        }

        uintptr_t EncodeLinkEditorId(size_t hash)
        {
            uintptr_t id = (static_cast<uintptr_t>(hash) << 2) | kLinkTypeTag;
            if (id == 0ull)
            {
                id = kLinkTypeTag;
            }
            return id;
        }

        std::unordered_map<MaterialGraphIds::PinKey, uintptr_t, PinKeyHash> s_PinForward;
        std::unordered_map<uintptr_t, MaterialGraphIds::PinKey> s_PinReverse;

        std::unordered_map<MaterialGraphIds::LinkKey, uintptr_t, LinkKeyHash> s_LinkForward;
        std::unordered_map<uintptr_t, MaterialGraphIds::LinkKey> s_LinkReverse;
    }

    bool MaterialGraphIds::PinKey::operator==(const PinKey& other) const
    {
        return Node == other.Node && Kind == other.Kind && Index == other.Index;
    }

    bool MaterialGraphIds::LinkKey::operator==(const LinkKey& other) const
    {
        return FromNode == other.FromNode && FromOutputIndex == other.FromOutputIndex &&
               ToNode == other.ToNode && ToInputIndex == other.ToInputIndex;
    }

    void MaterialGraphIds::Reset()
    {
        s_PinForward.clear();
        s_PinReverse.clear();
        s_LinkForward.clear();
        s_LinkReverse.clear();
    }

    ax::NodeEditor::NodeId MaterialGraphIds::ToNodeId(MaterialEdGraphNode* node)
    {
        return ax::NodeEditor::NodeId(node);
    }

    MaterialEdGraphNode* MaterialGraphIds::FromNodeId(ax::NodeEditor::NodeId id)
    {
        if (!id)
        {
            return nullptr;
        }

        const uintptr_t value = id.Get();
        if ((value & kTypeTagMask) != 0ull)
        {
            return nullptr;
        }

        return reinterpret_cast<MaterialEdGraphNode*>(value);
    }

    ax::NodeEditor::PinId MaterialGraphIds::ToPinId(const PinKey& key)
    {
        if (const auto found = s_PinForward.find(key); found != s_PinForward.end())
        {
            return ax::NodeEditor::PinId(reinterpret_cast<void*>(found->second));
        }

        size_t hash = HashPinKey(key);
        uintptr_t editorId = EncodePinEditorId(hash);
        for (;;)
        {
            const auto occupied = s_PinReverse.find(editorId);
            if (occupied == s_PinReverse.end() || occupied->second == key)
            {
                break;
            }

            hash = HashCombine(hash, editorId);
            editorId = EncodePinEditorId(hash);
        }

        s_PinForward.emplace(key, editorId);
        s_PinReverse[editorId] = key;
        return ax::NodeEditor::PinId(reinterpret_cast<void*>(editorId));
    }

    bool MaterialGraphIds::FromPinId(ax::NodeEditor::PinId id, PinKey& outKey)
    {
        if (!id)
        {
            return false;
        }

        const uintptr_t value = id.Get();
        if ((value & kTypeTagMask) != kPinTypeTag)
        {
            return false;
        }

        const auto found = s_PinReverse.find(value);
        if (found == s_PinReverse.end())
        {
            return false;
        }

        outKey = found->second;
        return outKey.Node != nullptr;
    }

    ax::NodeEditor::PinId MaterialGraphIds::ToPinId(
        MaterialEdGraphNode* node,
        ax::NodeEditor::PinKind kind,
        int32_t pinIndex)
    {
        return ToPinId(PinKey{node, kind, pinIndex});
    }

    bool MaterialGraphIds::FromPinId(
        ax::NodeEditor::PinId id,
        MaterialEdGraphNode*& outNode,
        ax::NodeEditor::PinKind& outKind,
        int32_t& outPinIndex)
    {
        PinKey key;
        if (!FromPinId(id, key))
        {
            return false;
        }

        outNode = key.Node;
        outKind = key.Kind;
        outPinIndex = key.Index;
        return true;
    }

    ax::NodeEditor::LinkId MaterialGraphIds::ToLinkId(const LinkKey& key)
    {
        if (const auto found = s_LinkForward.find(key); found != s_LinkForward.end())
        {
            return ax::NodeEditor::LinkId(reinterpret_cast<void*>(found->second));
        }

        size_t hash = HashLinkKey(key);
        uintptr_t editorId = EncodeLinkEditorId(hash);
        for (;;)
        {
            const auto occupied = s_LinkReverse.find(editorId);
            if (occupied == s_LinkReverse.end() || occupied->second == key)
            {
                break;
            }

            hash = HashCombine(hash, editorId);
            editorId = EncodeLinkEditorId(hash);
        }

        s_LinkForward.emplace(key, editorId);
        s_LinkReverse[editorId] = key;
        return ax::NodeEditor::LinkId(reinterpret_cast<void*>(editorId));
    }

    bool MaterialGraphIds::FromLinkId(ax::NodeEditor::LinkId id, LinkKey& outKey)
    {
        if (!id)
        {
            return false;
        }

        const uintptr_t value = id.Get();
        if ((value & kTypeTagMask) != kLinkTypeTag)
        {
            return false;
        }

        const auto found = s_LinkReverse.find(value);
        if (found == s_LinkReverse.end())
        {
            return false;
        }

        outKey = found->second;
        return outKey.FromNode != nullptr && outKey.ToNode != nullptr;
    }

    ax::NodeEditor::LinkId MaterialGraphIds::ToLinkId(
        MaterialEdGraphNode* fromNode,
        int32_t fromOutputIndex,
        MaterialEdGraphNode* toNode,
        int32_t toInputIndex)
    {
        return ToLinkId(LinkKey{fromNode, fromOutputIndex, toNode, toInputIndex});
    }

    bool MaterialGraphIds::FromLinkId(
        ax::NodeEditor::LinkId id,
        MaterialEdGraphNode*& outFromNode,
        int32_t& outFromOutputIndex,
        MaterialEdGraphNode*& outToNode,
        int32_t& outToInputIndex)
    {
        LinkKey key;
        if (!FromLinkId(id, key))
        {
            return false;
        }

        outFromNode = key.FromNode;
        outFromOutputIndex = key.FromOutputIndex;
        outToNode = key.ToNode;
        outToInputIndex = key.ToInputIndex;
        return true;
    }
}

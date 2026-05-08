#pragma once
#include "Core.h"

#include <algorithm>

namespace minEngine
{
    class EditorGraphNode;
    
    enum class EditorGraphPinDirection
    {
        Input,
        Output,
    };
    
    struct EditorGraphNodePin
    {
    public:
        std::string Name;
        EditorGraphPinDirection Direction;
        int32_t Index = -1;
        std::vector<EditorGraphNodePin*> LinkedTo;

        void SetOwner(EditorGraphNode* owner)
        {
            OwnerNode = owner;
        }

        EditorGraphNode* GetOwner() const
        {
            return OwnerNode;
        }

        bool MakeLinkTo(EditorGraphNodePin* other)
        {
            if (other && other != this && other->OwnerNode != OwnerNode && other->Direction != Direction)
            {
                if (std::find(LinkedTo.begin(), LinkedTo.end(), other) == LinkedTo.end())
                {
                    if(std::find(other->LinkedTo.begin(), other->LinkedTo.end(), this) == other->LinkedTo.end())
                    {
                        LinkedTo.push_back(other);
                        other->LinkedTo.push_back(this);
                        return true;
                    }
                }
            }
            return false;
        }

        bool BreakLinkTo(EditorGraphNodePin* other)
        {
            auto it = std::find(LinkedTo.begin(), LinkedTo.end(), other);
            if (it != LinkedTo.end())
            {
                LinkedTo.erase(it);
                auto otherIt = std::find(other->LinkedTo.begin(), other->LinkedTo.end(), this);
                if (otherIt != other->LinkedTo.end())                
                {
                    other->LinkedTo.erase(otherIt);
                }
                return true;
            }
            return false;
        }

    private:
        EditorGraphNode* OwnerNode = nullptr;
    };
}
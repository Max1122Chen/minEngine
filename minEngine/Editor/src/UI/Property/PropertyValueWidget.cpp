#include "UI/Property/PropertyValueWidget.h"

#include "UI/Property/ColorWidget.h"
#include "UI/Property/PropertyEnumWidget.h"
#include "UI/Property/PropertyPrimitiveWidgets.h"

#include "Runtime/Core/Math/Color.h"
#include "Runtime/Core/Reflection/Reflection.h"

namespace minEngine
{
    bool PropertyValueWidget::IsLinearColorStruct(const Reflection::MEClass* valueClass)
    {
        return valueClass != nullptr && valueClass->IsA(LinearColor::StaticClass());
    }

    bool PropertyValueWidget::Draw(const Reflection::MEProperty& property,
                                   void* propertyPtr,
                                   float itemWidth)
    {
        switch (property.GetCategory())
        {
            case Reflection::MEPropertyCategory::Primitive:
            {
                const Reflection::MEPrimitiveProperty& primitiveProperty =
                    static_cast<const Reflection::MEPrimitiveProperty&>(property);
                if (primitiveProperty.IsEnum())
                {
                    return PropertyEnumWidget::Draw(primitiveProperty, propertyPtr, itemWidth);
                }

                return PropertyPrimitiveWidgets::Draw(primitiveProperty, propertyPtr, itemWidth);
            }

            case Reflection::MEPropertyCategory::Object:
            {
                const Reflection::MEObjectProperty& objectProperty =
                    static_cast<const Reflection::MEObjectProperty&>(property);
                if (IsLinearColorStruct(objectProperty.GetValueClass()))
                {
                    return ColorWidget::DrawLinearColor(static_cast<LinearColor*>(propertyPtr), itemWidth);
                }

                return false;
            }

            default:
                return false;
        }
    }
}

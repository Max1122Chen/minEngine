#include "Reflection.h"

namespace minEngine::Reflection
{
	bool ForEachFieldInHierarchy(const std::string& rootTypeName, const FieldVisitorFn& visitor)
	{
		return ReflectionSystem::Get().ForEachFieldInHierarchy(rootTypeName, visitor);
	}

	const void* CastObjectToType(const void* object, const std::string& sourceTypeName, const std::string& targetTypeName)
	{
		return ReflectionSystem::Get().CastObjectToType(object, sourceTypeName, targetTypeName);
	}

	void* CastObjectToType(void* object, const std::string& sourceTypeName, const std::string& targetTypeName)
	{
		return ReflectionSystem::Get().CastObjectToType(object, sourceTypeName, targetTypeName);
	}
}
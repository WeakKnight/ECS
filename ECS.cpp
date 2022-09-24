#include "ECS.h"

namespace ECS
{
	static Registry* sRegistry = nullptr;

	void Init()
	{
		if (sRegistry == nullptr)
		{
			sRegistry = new Registry();
		}
	}

	void Release()
	{
		if (sRegistry != nullptr)
		{
			delete sRegistry;
			sRegistry = nullptr;
		}
	}

	Registry::~Registry()
	{
		sRegistry->Release();
	}

	Entity Entity::Create()
	{
		Entity result = {};
		result.mId = sRegistry->CreateEntity();
		return result;
	}

	void Entity::Destroy(Entity entity)
	{
		sRegistry->DestroyEntity(entity.mId);
	}

	Registry& Entity::GetRegistry()
	{
		return *sRegistry;
	}
} // namespace ECS
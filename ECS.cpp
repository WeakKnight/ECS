#include "ECS.h"

static Registry sRegistry; 

Entity Entity::Create()
{
	Entity result = {};
	result.id =  sRegistry.CreateEntity();
	return result;
}

void Entity::Destroy(Entity entity)
{
	sRegistry.DestroyEntity(entity.id);
}

void Entity::Release()
{
	sRegistry.Release();
}

Registry& Entity::GetRegistry()
{
    return sRegistry;
}

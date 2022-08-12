#pragma once
#include <stdint.h>
#include <atomic>
#include <vcruntime.h>
#include <vector>
#include <unordered_map>
#include <typeinfo>
#include <functional>
#include <cassert>

typedef uint32_t EntityID;
#define ENTITY_NULL 0

template<typename T>
struct ComponentManager
{
	std::unordered_map<EntityID, size_t> componentLookups;
	std::unordered_map<T*, EntityID> entityLookups;
	std::vector<EntityID> owners;
	std::vector<T> components;
	size_t typeIndex;

	T* AddComponent(EntityID entity)
	{
		components.push_back({});
		owners.push_back(entity);
		componentLookups[entity] = components.size() - 1;
		entityLookups[&components.back()] = entity;
		return &components.back();
	}

	T* GetComponent(EntityID entity)
	{
		auto itemFound = componentLookups.find(entity);
		if (itemFound == componentLookups.cend())
		{
			return nullptr;
		}
		return &components[itemFound->second];
	}

	void RemoveComponent(EntityID entity)
	{
		size_t compIndex = componentLookups[entity];
		componentLookups.erase(entity);
		entityLookups.erase(&components[compIndex]);

		owners[compIndex] = owners.back();
		owners.pop_back();
		if (owners.size() >= 1)
		{
			componentLookups[owners[compIndex]] = compIndex;
		}

		components[compIndex] = components.back();
		entityLookups.erase(&components.back());
		components.pop_back();
		if (components.size() >= 1)
		{
			entityLookups[&components[compIndex]] = owners[compIndex];
		}
	}

	bool ContainComponent(EntityID entity) const
	{
		return componentLookups.find(entity) != componentLookups.cend();
	}
};

struct Registry
{
	std::atomic<EntityID> mEntityIndex;
	std::unordered_map<size_t, void*> mComponentManagers;
	std::vector<std::function<void()>> mReleaseFunctions;
	std::unordered_map<EntityID, std::unordered_map<size_t, std::function<void()>>> mEntityDeleters;

	EntityID CreateEntity()
	{
		EntityID id = ++mEntityIndex;
		mEntityDeleters[id] = {};
		return id;
	}

	void DestroyEntity(EntityID& entity)
	{
		for(auto& pair : mEntityDeleters[entity])
		{
			pair.second();
		}
		
		mEntityDeleters[entity].clear();

		assert(mEntityDeleters[entity].size() == 0);

		entity = ENTITY_NULL;
	}

	void Release()
	{
		for (auto& func : mReleaseFunctions)
		{
			func();
		}
		assert(mComponentManagers.size() == 0);

		mReleaseFunctions.clear();
		mEntityDeleters.clear();
	}

	template <typename T>
	ComponentManager<T>* GetComponentManager()
	{
		size_t typeIndex = typeid(T).hash_code();
		if (mComponentManagers.find(typeIndex) == mComponentManagers.cend())
		{
			ComponentManager<T>* comMgr = new ComponentManager<T>();
			comMgr->typeIndex = typeIndex;
			mComponentManagers[typeIndex] = comMgr;

			mReleaseFunctions.push_back([=]() 
			{
				delete comMgr;
				mComponentManagers.erase(typeIndex);
			});
		}
		ComponentManager<T>* compMgr = (ComponentManager<T>*)mComponentManagers[typeIndex];
		return compMgr;
	}

	template <typename T>
	T* AddComponent(EntityID entity)
	{
		ComponentManager<T>* compMgr = GetComponentManager<T>();
		auto deleter = [=]()
		{
			RemoveComponent<T>(entity);
		};
		mEntityDeleters[entity][compMgr->typeIndex] = deleter;
		return compMgr->AddComponent(entity);
	}

	template <typename T>
	T* GetComponent(EntityID entity)
	{
		ComponentManager<T>* compMgr = GetComponentManager<T>();
		return compMgr->GetComponent(entity);
	}

	template <typename T>
	void RemoveComponent(EntityID entity)
	{
		ComponentManager<T>* compMgr = GetComponentManager<T>();
		compMgr->RemoveComponent(entity);
	}

	template <typename T>
	bool ContainComponent(EntityID entity)
	{
		ComponentManager<T>* compMgr = GetComponentManager<T>();
		return compMgr->ContainComponent(entity);
	}
};

class Entity
{
public:
	static Entity Create();
	static void Destroy(Entity entity);
	static void Release();

    Entity()    
    {
        id = ENTITY_NULL;
    }

    Entity(EntityID entityId)
	{
		id = entityId;
    }

    Entity(const Entity& other)
    {
        id = other.id;
    }

    static Registry& GetRegistry();

    operator EntityID()
    {
        return id;
    }

    bool operator!() const
    {
        return id == ENTITY_NULL;
    }

    explicit operator bool() const
    {
        return id != ENTITY_NULL;
    }

    bool operator==(const Entity& other) const
    {
        return id == other.id;
    }

    bool operator!=(const Entity& other) const
    {
        return id != other.id;
    }

    template <typename T>
    T* AddComponent()
    {
        return GetRegistry().AddComponent<T>(id);
    }

    template <typename T>
    T* GetComponent() const
    {
        return GetRegistry().GetComponent<T>(id);
    }

    template <typename T>
    void RemoveComponent()
    {
        GetRegistry().RemoveComponent<T>(id);
    }

    template <typename T>
    bool ContainComponent()
    {
        return GetRegistry().ContainComponent<T>(id);
    }

	template <typename T, typename U>
	bool ContainComponent()
	{
		return GetRegistry().ContainComponent<T>(id) && GetRegistry().ContainComponent<U>(id);
	}

	template <typename T>
	static void ForEach(std::function<void(T*)> func)
	{
		ComponentManager<T>* compMgr = GetRegistry().GetComponentManager<T>();
		for (size_t i = 0; i < compMgr->components.size(); i++)
		{
			T* component = &compMgr->components[i];
			func(component);
		}
	}

    template <typename T>
	static void ForEach(std::function<void(Entity, T*)> func)
	{
		ComponentManager<T>* compMgr = GetRegistry().GetComponentManager<T>();
		for (size_t i = 0; i < compMgr->components.size(); i++)
		{
			Entity entity = compMgr->owners[i];
			T* component = &compMgr->components[i];
			func(entity, component);
		}
	}

	template <typename T, typename U>
	static void ForEach(std::function<void(T*, U*)> func)
	{
		ComponentManager<T>* compMgr = GetRegistry().GetComponentManager<T>();
		for (size_t i = 0; i < compMgr->components.size(); i++)
		{
			Entity entity = compMgr->owners[i];
			T* componentT = &compMgr->components[i];
			U* componentU = entity.GetComponent<U>();
			if (componentU)
			{
				func(componentT, componentU);
			}
		}
	}

private:
	EntityID id;
};

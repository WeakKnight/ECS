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

template <typename T>
struct ComponentManager
{
	std::unordered_map<EntityID, size_t> componentLookups;
	std::unordered_map<T*, EntityID> entityLookups;
	std::vector<EntityID> owners;
	std::vector<T> components;
	size_t typeIndex;

	template <typename... Args>
	T* AddComponent(EntityID entity, Args&&... args)
	{
		components.push_back(T(std::forward<Args>(args)...));
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
		EntityID mId = ++mEntityIndex;
		mEntityDeleters[mId] = {};
		return mId;
	}

	void DestroyEntity(EntityID& entity)
	{
		for (auto& pair : mEntityDeleters[entity])
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

			mReleaseFunctions.push_back([=]() {
				delete comMgr;
				mComponentManagers.erase(typeIndex);
			});
		}
		ComponentManager<T>* compMgr = (ComponentManager<T>*)mComponentManagers[typeIndex];
		return compMgr;
	}

	template <typename T, typename... Args>
	T* AddComponent(EntityID entity, Args&&... args)
	{
		ComponentManager<T>* compMgr = GetComponentManager<T>();
		auto deleter = [=]() {
			RemoveComponent<T>(entity);
		};
		mEntityDeleters[entity][compMgr->typeIndex] = deleter;
		return compMgr->AddComponent(entity, std::forward<Args>(args)...);
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
		mId = ENTITY_NULL;
	}

	Entity(EntityID entityId)
	{
		mId = entityId;
	}

	Entity(const Entity& other)
	{
		mId = other.mId;
	}

	operator EntityID()
	{
		return mId;
	}

	bool operator!() const
	{
		return mId == ENTITY_NULL;
	}

	explicit operator bool() const
	{
		return mId != ENTITY_NULL;
	}

	bool operator==(const Entity& other) const
	{
		return mId == other.mId;
	}

	bool operator!=(const Entity& other) const
	{
		return mId != other.mId;
	}

	template <typename T, typename... Args>
	T* AddComponent(Args&&... args)
	{
		return GetRegistry().AddComponent<T>(mId, std::forward<Args>(args)...);
	}

	template <typename T>
	T* GetComponent() const
	{
		return GetRegistry().GetComponent<T>(mId);
	}

	template <typename T>
	void RemoveComponent()
	{
		GetRegistry().RemoveComponent<T>(mId);
	}

	template <typename T>
	bool ContainComponent()
	{
		return GetRegistry().ContainComponent<T>(mId);
	}

	template <typename T, typename U>
	bool ContainComponent()
	{
		return GetRegistry().ContainComponent<T>(mId) && GetRegistry().ContainComponent<U>(mId);
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
	static void ForEach(std::function<void(Entity, T*, U*)> func)
	{
		ComponentManager<T>* compMgrT = GetRegistry().GetComponentManager<T>();
		ComponentManager<U>* compMgrU = GetRegistry().GetComponentManager<U>();
		if (compMgrT->components.size() <= compMgrU->components.size())
		{
			ForEach<T>([&func](Entity entity, T* componentT) {
				U* componentU = entity.GetComponent<U>();
				if (componentU)
				{
					func(entity, componentT, componentU);
				}
			});
		}
		else
		{
			ForEach<U>([&func](Entity entity, U* componentU) {
				T* componentT = entity.GetComponent<T>();
				if (componentT)
				{
					func(entity, componentT, componentU);
				}
			});
		}
	}

	template <typename T, typename U, typename V>
	static void ForEach(std::function<void(Entity, T*, U*, V*)> func)
	{
		size_t compMgrTCount = GetRegistry().GetComponentManager<T>()->components.size();
		size_t compMgrUCount = GetRegistry().GetComponentManager<U>()->components.size();
		size_t compMgrVCount = GetRegistry().GetComponentManager<V>()->components.size();
		size_t minCount = compMgrTCount < compMgrUCount ? compMgrTCount : compMgrUCount;
		minCount = minCount < compMgrVCount ? minCount : compMgrVCount;
		if (minCount == compMgrTCount)
		{
			ForEach<T>([&func](Entity entity, T* componentT) {
				U* componentU = entity.GetComponent<U>();
				V* componentV = entity.GetComponent<V>();
				if (componentU && componentV)
				{
					func(entity, componentT, componentU, componentV);
				}
			});
		}
		else if (minCount == compMgrUCount)
		{
			ForEach<U>([&func](Entity entity, U* componentU) {
				T* componentT = entity.GetComponent<T>();
				V* componentV = entity.GetComponent<V>();
				if (componentT && componentV)
				{
					func(entity, componentT, componentU, componentV);
				};
			});
		}
		else if (minCount == compMgrVCount)
		{
			ForEach<V>([&func](Entity entity, V* componentV) {
				T* componentT = entity.GetComponent<T>();
				U* componentU = entity.GetComponent<U>();
				if (componentT && componentU)
				{
					func(entity, componentT, componentU, componentV);
				}
			});
		}
	}

	template <typename T>
	static void ForEach(std::function<void(T*)> func)
	{
		ForEach<T>([&func](Entity, T* component) {
			func(component);
		});
	}

	template <typename T, typename U>
	static void ForEach(std::function<void(T*, U*)> func)
	{
		ForEach<T, U>([&func](Entity, T* componentT, U* componentU) {
			func(componentT, componentU);
		});
	}

	template <typename T, typename U, typename V>
	static void ForEach(std::function<void(T*, U*, V*)> func)
	{
		ForEach<T, U, V>([&func](Entity, T* componentT, U* componentU, V* componentV) {
			func(componentT, componentU, componentV);
		});
	}

private:
	static Registry& GetRegistry();
	
	EntityID mId;
};

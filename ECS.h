#pragma once
#include <stdint.h>
#include <atomic>
#include <vcruntime.h>
#include <vector>
#include <unordered_map>
#include <map>
#include <typeinfo>
#include <functional>
#include <cassert>

#define COMPONENT_CTOR inline void ComponentInit

namespace ECS
{
	void Init();
	void Release();

	typedef uint32_t EntityID;
	typedef uint64_t ComponentHash;
#define ENTITY_NULL 0

	template <typename T>
	struct ComponentHandle
	{
		operator T*()
		{
			return Get();
		}

		operator const T*() const
		{
			return Get();
		}

		bool operator!() const
		{
			return (Get() == nullptr);
		}

		explicit operator bool() const
		{
			return (Get() != nullptr);
		}

		T* operator->() { return Get(); }
		const T* operator->() const { return Get(); }

		T* Get()
		{
			return Entity::GetRegistry().GetComponentManager<T>()->components.Get(*this);
		}

		ComponentHash Hash()
		{
			return (uint64_t)index + generation;
		}

		uint32_t index = ~0u;
		uint32_t generation = 0u;
	};

	template <typename T>
	struct ComponentPool
	{
		T* mComponents;
		std::vector<EntityID> mOwners;
		std::vector<uint32_t> mGenerations;
		std::vector<uint32_t> mFreeSlots;
		size_t mCapacity = 1024;

		ComponentPool()
		{
			mComponents = (T*)malloc(sizeof(T) * mCapacity);
			std::uninitialized_default_construct_n(mComponents, mCapacity);

			mOwners.resize(mCapacity);
			mGenerations.resize(mCapacity);
			mFreeSlots.reserve(mCapacity);

			for (size_t i = 0; i < mCapacity; i++)
			{
				mFreeSlots.push_back((EntityID)i);
			}
		}

		~ComponentPool()
		{
			free(mComponents);
		}

		template <typename... Args>
		ComponentHandle<T> Alloc(EntityID owner, Args&&... args)
		{
			assert(mGenerations.size() == mOwners.size());

			if (mFreeSlots.size() <= 0u)
			{
				Grow();
			}

			ComponentHandle<T> handle;
			handle.index = mFreeSlots.back();
			handle.generation = mGenerations[handle.index];

			mOwners[handle.index] = owner;
			mComponents[handle.index] = T();
			mComponents[handle.index].ComponentInit(std::forward<Args>(args)...);

			mFreeSlots.pop_back();

			return handle;
		}

		void Free(ComponentHandle<T> handle)
		{
			if (!IsValid(handle))
			{
				return;
			}

			mGenerations[handle.index]++;
			mFreeSlots.push_back(handle.index);
		}

		bool IsValid(ComponentHandle<T> handle)
		{
			return handle.index != ~0u && mGenerations[handle.index] == handle.generation;
		}

		void Grow()
		{
			size_t newCapacity = mCapacity * 2;

			mFreeSlots.reserve(newCapacity);
			for (size_t i = mCapacity; i < newCapacity; i++)
			{
				mFreeSlots.push_back((EntityID)i);
			}

			T* newComponents = (T*)malloc(sizeof(T) * newCapacity);
			std::uninitialized_default_construct_n(newComponents, newCapacity);

			memcpy(newComponents, mComponents, sizeof(T) * mCapacity);
			free(mComponents);
			mComponents = newComponents;

			mOwners.resize(newCapacity);

			mGenerations.resize(newCapacity);

			mCapacity = newCapacity;
		}

		T* Get(ComponentHandle<T> handle)
		{
			if (!IsValid(handle))
			{
				return nullptr;
			}

			return &mComponents[handle.index];
		}

		EntityID Owner(ComponentHandle<T> handle)
		{
			if (!IsValid(handle))
			{
				return ENTITY_NULL;
			}

			return mOwners[handle.index];
		}
	};

	template <typename T>
	struct ComponentManager
	{
		std::map<EntityID, ComponentHandle<T>> componentLookups;
		ComponentPool<T> components;

		size_t typeIndex;

		EntityID GetEntity(ComponentHandle<T> component)
		{
			return components.Owner(component);
		}

		template <typename... Args>
		ComponentHandle<T> AddComponent(EntityID entity, Args&&... args)
		{
			ComponentHandle<T> result = components.Alloc(entity, std::forward<Args>(args)...);
			componentLookups[entity] = result;
			return result;
		}

		ComponentHandle<T> GetComponent(EntityID entity)
		{
			auto itemFound = componentLookups.find(entity);
			if (itemFound == componentLookups.cend())
			{
				return {};
			}
			return itemFound->second;
		}

		void RemoveComponent(EntityID entity)
		{
			ComponentHandle<T> handle = componentLookups[entity];
			componentLookups.erase(entity);
			components.Free(handle);
		}

		bool ContainComponent(EntityID entity) const
		{
			return componentLookups.find(entity) != componentLookups.cend();
		}
	};

	struct Registry
	{
		~Registry();

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
		ComponentHandle<T> AddComponent(EntityID entity, Args&&... args)
		{
			ComponentManager<T>* compMgr = GetComponentManager<T>();
			auto deleter = [=]() {
				RemoveComponent<T>(entity);
			};
			mEntityDeleters[entity][compMgr->typeIndex] = deleter;
			return compMgr->AddComponent(entity, std::forward<Args>(args)...);
		}

		template <typename T>
		ComponentHandle<T> GetComponent(EntityID entity)
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

		template <typename T>
		EntityID FromComponent(ComponentHandle<T> component)
		{
			ComponentManager<T>* compMgr = GetComponentManager<T>();
			EntityID entity = compMgr->GetEntity(component);
			return entity;
		}
	};

	class Entity
	{
	public:
		static Entity Create();
		static void Destroy(Entity entity);

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
		ComponentHandle<T> AddComponent(Args&&... args)
		{
			return GetRegistry().AddComponent<T>(mId, std::forward<Args>(args)...);
		}

		template <typename T>
		ComponentHandle<T> GetComponent() const
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
		static Entity FromComponent(ComponentHandle<T> component)
		{
			return GetRegistry().FromComponent(component);
		}

		template <typename T>
		static void ForEach(std::function<void(Entity, ComponentHandle<T>)> func)
		{
			ComponentManager<T>* compMgr = GetRegistry().GetComponentManager<T>();
			for (auto& pair : compMgr->componentLookups)
			{
				func(pair.first, pair.second);
			}
		}

		template <typename T, typename U>
		static void ForEach(std::function<void(Entity, ComponentHandle<T>, ComponentHandle<U>)> func)
		{
			ComponentManager<T>* compMgrT = GetRegistry().GetComponentManager<T>();
			ComponentManager<U>* compMgrU = GetRegistry().GetComponentManager<U>();
			if (compMgrT->componentLookups.size() <= compMgrU->componentLookups.size())
			{
				ForEach<T>([&func](Entity entity, ComponentHandle<T> componentT) {
					ComponentHandle<U> componentU = entity.GetComponent<U>();
					if (componentU)
					{
						func(entity, componentT, componentU);
					}
				});
			}
			else
			{
				ForEach<U>([&func](Entity entity, ComponentHandle<U> componentU) {
					ComponentHandle<T> componentT = entity.GetComponent<T>();
					if (componentT)
					{
						func(entity, componentT, componentU);
					}
				});
			}
		}

		template <typename T, typename U, typename V>
		static void ForEach(std::function<void(Entity, ComponentHandle<T>, ComponentHandle<U>, ComponentHandle<V>)> func)
		{
			size_t compMgrTCount = GetRegistry().GetComponentManager<T>()->componentLookups.size();
			size_t compMgrUCount = GetRegistry().GetComponentManager<U>()->componentLookups.size();
			size_t compMgrVCount = GetRegistry().GetComponentManager<V>()->componentLookups.size();
			size_t minCount = compMgrTCount < compMgrUCount ? compMgrTCount : compMgrUCount;
			minCount = minCount < compMgrVCount ? minCount : compMgrVCount;
			if (minCount == compMgrTCount)
			{
				ForEach<T>([&func](Entity entity, ComponentHandle<T> componentT) {
					ComponentHandle<U> componentU = entity.GetComponent<U>();
					ComponentHandle<V> componentV = entity.GetComponent<V>();
					if (componentU && componentV)
					{
						func(entity, componentT, componentU, componentV);
					}
				});
			}
			else if (minCount == compMgrUCount)
			{
				ForEach<U>([&func](Entity entity, ComponentHandle<U> componentU) {
					ComponentHandle<T> componentT = entity.GetComponent<T>();
					ComponentHandle<V> componentV = entity.GetComponent<V>();
					if (componentT && componentV)
					{
						func(entity, componentT, componentU, componentV);
					};
				});
			}
			else if (minCount == compMgrVCount)
			{
				ForEach<V>([&func](Entity entity, ComponentHandle<V> componentV) {
					ComponentHandle<T> componentT = entity.GetComponent<T>();
					ComponentHandle<U> componentU = entity.GetComponent<U>();
					if (componentT && componentU)
					{
						func(entity, componentT, componentU, componentV);
					}
				});
			}
		}

		template <typename T>
		static void ForEach(std::function<void(ComponentHandle<T>)> func)
		{
			ForEach<T>([&func](Entity, ComponentHandle<T> component) {
				func(component);
			});
		}

		template <typename T, typename U>
		static void ForEach(std::function<void(ComponentHandle<T>, ComponentHandle<U>)> func)
		{
			ForEach<T, U>([&func](Entity, ComponentHandle<T> componentT, ComponentHandle<U> componentU) {
				func(componentT, componentU);
			});
		}

		template <typename T, typename U, typename V>
		static void ForEach(std::function<void(ComponentHandle<T>, ComponentHandle<U>, ComponentHandle<V>)> func)
		{
			ForEach<T, U, V>([&func](Entity, ComponentHandle<T> componentT, ComponentHandle<U> componentU, ComponentHandle<V> componentV) {
				func(componentT, componentU, componentV);
			});
		}

		static Registry& GetRegistry();

	private:
		EntityID mId;
	};
} // namespace ECS
// EditorSceneSnapshot.cpp

#include "Editor/EditorSceneSnapshot.h"

#include "Core/NeneLogger.h"
#include "ECS/Events/SceneRestoredEvent.h"
#include "ECS/World.h"

#include <EASTL/algorithm.h>
#include <EASTL/sort.h>

namespace NeneEngine
{
	namespace
	{
		bool IsSceneEntity(const eastl::vector<ECS::Entity>& windowCameras, ECS::Entity entity)
		{
			return eastl::find(windowCameras.begin(), windowCameras.end(), entity) == windowCameras.end();
		}

		template <typename Component>
		void CaptureComponents(const entt::registry& registry,
		                       eastl::vector<eastl::pair<ECS::Entity, Component>>& values,
		                       const eastl::vector<ECS::Entity>& windowCameras)
		{
			values.clear();
			for (auto [entity, component] : registry.view<const Component>().each())
			{
				if (IsSceneEntity(windowCameras, entity)) values.emplace_back(entity, component);
			}
			eastl::sort(values.begin(), values.end(),
			            [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
		}

		template <typename Component>
		size_t RestoreComponents(entt::registry& registry,
		                         const eastl::vector<eastl::pair<ECS::Entity, Component>>& values,
		                         const eastl::vector<ECS::Entity>& windowCameras)
		{
			eastl::vector<ECS::Entity> addedDuringPlay;
			for (const ECS::Entity entity : registry.view<const Component>())
			{
				if (!IsSceneEntity(windowCameras, entity)) continue;

				const auto it =
				    eastl::lower_bound(values.begin(), values.end(), entity,
				                       [](const auto& entry, ECS::Entity key) { return entry.first < key; });
				const bool hadComponent = it != values.end() && it->first == entity;
				if (!hadComponent) addedDuringPlay.push_back(entity);
			}
			for (const ECS::Entity entity : addedDuringPlay) registry.remove<Component>(entity);

			size_t missingCount = 0;
			for (const auto& [entity, value] : values)
			{
				if (!registry.valid(entity))
				{
					++missingCount;
					continue;
				}

				registry.emplace_or_replace<Component>(entity, value);
			}
			return missingCount;
		}
	} // namespace

	void EditorSceneSnapshot::Capture(const ECS::World& world, const eastl::vector<ECS::Entity>& windowCameras)
	{
		Clear();
		m_windowCameras = windowCameras;
		const entt::registry& registry = world.GetRegistry();

		for (const ECS::Entity entity : registry.view<entt::entity>())
		{
			if (IsSceneEntity(m_windowCameras, entity)) m_entities.push_back(entity);
		}
		eastl::sort(m_entities.begin(), m_entities.end());

		eastl::apply([&](auto&... stores) { (CaptureComponents(registry, stores.values, m_windowCameras), ...); },
		             m_stores);
		m_hasCapture = true;

		NENE_LOG_INFO("Editor snapshot captured: {} entities", m_entities.size());
	}

	void EditorSceneSnapshot::Restore(ECS::World& world)
	{
		if (!m_hasCapture) return;

		entt::registry& registry = world.GetRegistry();
		const auto wasCreatedDuringPlay = [&](ECS::Entity entity)
		{ return IsSceneEntity(m_windowCameras, entity) && !WasCaptured(entity); };

		eastl::vector<ECS::Entity> createdDuringPlay;
		for (const ECS::Entity entity : registry.view<entt::entity>())
		{
			if (wasCreatedDuringPlay(entity)) createdDuringPlay.push_back(entity);
		}

		for (const ECS::Entity entity : createdDuringPlay)
		{
			auto* hierarchy = registry.try_get<ECS::HierarchyComponent>(entity);
			if (hierarchy == nullptr) continue;
			eastl::erase_if(hierarchy->children, [&](ECS::Entity child)
			{
				if (wasCreatedDuringPlay(child)) return false;
				auto* childHierarchy = registry.try_get<ECS::HierarchyComponent>(child);
				if (childHierarchy != nullptr && childHierarchy->parent == entity)
					childHierarchy->parent = ECS::NullEntity;
				return true;
			});
		}

		for (const ECS::Entity entity : createdDuringPlay) world.DestroyEntity(entity);

		size_t recreatedCount = 0;
		for (const ECS::Entity entity : m_entities)
		{
			if (registry.valid(entity)) continue;

			const ECS::Entity recreated = registry.create(entity);
			if (recreated == entity)
			{
				++recreatedCount;
				continue;
			}

			registry.destroy(recreated);
			NENE_LOG_WARN("Editor snapshot: failed to recreate entity {}", entt::to_integral(entity));
		}

		size_t missingCount = 0;
		eastl::apply(
		    [&](const auto&... stores)
		    { ((missingCount += RestoreComponents(registry, stores.values, m_windowCameras)), ...); },
		    m_stores);

		if (missingCount > 0)
			NENE_LOG_WARN("Editor snapshot: {} components belong to entities destroyed during play and were not "
			              "restored",
			              missingCount);

		world.GetEventBus().Publish(ECS::SceneRestoredEvent{});

		NENE_LOG_INFO("Editor snapshot restored, {} entities created during play removed, {} destroyed during play "
		              "recreated",
		              createdDuringPlay.size(), recreatedCount);
		Clear();
	}

	void EditorSceneSnapshot::Clear()
	{
		m_hasCapture = false;
		m_entities.clear();
		m_windowCameras.clear();
		eastl::apply([](auto&... stores) { (stores.values.clear(), ...); }, m_stores);
	}

	bool EditorSceneSnapshot::WasCaptured(ECS::Entity entity) const
	{
		return eastl::binary_search(m_entities.begin(), m_entities.end(), entity);
	}

} // namespace NeneEngine

// EditorSceneSnapshot.cpp

#include "Editor/EditorSceneSnapshot.h"

#include "Core/NeneLogger.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/World.h"

#include <EASTL/algorithm.h>
#include <EASTL/sort.h>

namespace NeneEngine
{
	namespace
	{
		bool IsSceneEntity(const entt::registry& registry, ECS::Entity entity)
		{
			return !registry.all_of<ECS::CameraComponent>(entity);
		}

		template <typename Component>
		void CaptureComponents(const entt::registry& registry,
		                       eastl::vector<eastl::pair<ECS::Entity, Component>>& values)
		{
			values.clear();
			for (auto [entity, component] : registry.view<const Component>().each())
			{
				if (IsSceneEntity(registry, entity)) values.emplace_back(entity, component);
			}
			eastl::sort(values.begin(), values.end(),
			            [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
		}

		template <typename Component>
		size_t RestoreComponents(entt::registry& registry,
		                         const eastl::vector<eastl::pair<ECS::Entity, Component>>& values)
		{
			eastl::vector<ECS::Entity> addedDuringPlay;
			for (const ECS::Entity entity : registry.view<const Component>())
			{
				if (!IsSceneEntity(registry, entity)) continue;

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

	void EditorSceneSnapshot::Capture(const ECS::World& world)
	{
		const entt::registry& registry = world.GetRegistry();

		m_entities.clear();
		for (const ECS::Entity entity : registry.view<entt::entity>())
		{
			if (IsSceneEntity(registry, entity)) m_entities.push_back(entity);
		}
		eastl::sort(m_entities.begin(), m_entities.end());

		eastl::apply([&registry](auto&... stores) { (CaptureComponents(registry, stores.values), ...); }, m_stores);

		NENE_LOG_INFO("Editor snapshot captured: {} entities", m_entities.size());
	}

	void EditorSceneSnapshot::Restore(ECS::World& world)
	{
		if (IsEmpty()) return;

		entt::registry& registry = world.GetRegistry();

		eastl::vector<ECS::Entity> createdDuringPlay;
		for (const ECS::Entity entity : registry.view<entt::entity>())
		{
			if (IsSceneEntity(registry, entity) && !WasCaptured(entity)) createdDuringPlay.push_back(entity);
		}
		for (const ECS::Entity entity : createdDuringPlay) world.DestroyEntity(entity);

		size_t missingCount = 0;
		eastl::apply(
		    [&](const auto&... stores) { ((missingCount += RestoreComponents(registry, stores.values)), ...); },
		    m_stores);

		if (missingCount > 0)
			NENE_LOG_WARN("Editor snapshot: {} components belong to entities destroyed during play and were not "
			              "restored",
			              missingCount);

		NENE_LOG_INFO("Editor snapshot restored, {} entities created during play removed", createdDuringPlay.size());
		Clear();
	}

	void EditorSceneSnapshot::Clear()
	{
		m_entities.clear();
		eastl::apply([](auto&... stores) { (stores.values.clear(), ...); }, m_stores);
	}

	bool EditorSceneSnapshot::WasCaptured(ECS::Entity entity) const
	{
		return eastl::binary_search(m_entities.begin(), m_entities.end(), entity);
	}

} // namespace NeneEngine

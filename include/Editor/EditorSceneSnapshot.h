// EditorSceneSnapshot.h

#pragma once

#include "ECS/Components/ColliderComponent.h"
#include "ECS/Components/ContactStateComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/MovementComponent.h"
#include "ECS/Components/PlayerControllerComponent.h"
#include "ECS/Components/PrimitiveControlComponent.h"
#include "ECS/Components/RigidbodyComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Entity.h"

#include <EASTL/tuple.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>
#include <cstddef>

namespace NeneEngine
{
	namespace ECS
	{
		class World;
	}

	class EditorSceneSnapshot final
	{
	  public:
		void Capture(const ECS::World& world);
		void Restore(ECS::World& world);
		void Clear();

		[[nodiscard]] bool IsEmpty() const { return m_entities.empty(); }

	  private:
		template <typename Component> struct ComponentStore
		{
			eastl::vector<eastl::pair<ECS::Entity, Component>> values;
		};

		using ComponentStores =
		    eastl::tuple<ComponentStore<ECS::TagComponent>, ComponentStore<ECS::TransformComponent>,
		                 ComponentStore<ECS::HierarchyComponent>, ComponentStore<ECS::MeshRendererComponent>,
		                 ComponentStore<ECS::ColliderComponent>, ComponentStore<ECS::RigidbodyComponent>,
		                 ComponentStore<ECS::ContactStateComponent>, ComponentStore<ECS::MovementComponent>,
		                 ComponentStore<ECS::PlayerControllerComponent>,
		                 ComponentStore<ECS::PrimitiveControlComponent>>;

		[[nodiscard]] bool WasCaptured(ECS::Entity entity) const;

		eastl::vector<ECS::Entity> m_entities;
		ComponentStores m_stores;
	};

} // namespace NeneEngine

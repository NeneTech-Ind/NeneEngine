// HierarchyComponent.h

#pragma once

#include "ECS/Entity.h"

#include <EASTL/vector.h>

namespace NeneEngine::ECS
{

	struct HierarchyComponent
	{
		Entity parent = NullEntity;
		eastl::vector<Entity> children{};
	};

} // namespace NeneEngine::ECS

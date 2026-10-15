// EditorHistory.h

#pragma once

#include "ECS/Components/TransformComponent.h"
#include "ECS/Entity.h"

namespace NeneEngine
{

	class EditorHistory final
	{
	  public:
		void RecordTransformChange(ECS::Entity /*entity*/, const ECS::TransformComponent& /*before*/,
		                           const ECS::TransformComponent& /*after*/)
		{
		}
	};

} // namespace NeneEngine

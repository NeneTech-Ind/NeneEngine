// MeshRendererComponent.h

#pragma once

#include "Graphics/Runtime/RenderTypes.h"

namespace NeneEngine::ECS
{

	struct MeshRendererComponent
	{
		PrimitiveType primitiveType = PrimitiveType::Triangle;
		bool visible = true;
		float cullingRadius = 1.0f;
		glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
	};

} // namespace NeneEngine::ECS

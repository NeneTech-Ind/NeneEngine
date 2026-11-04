// MeshRendererComponent.h

#pragma once

#include "Graphics/Runtime/RenderTypes.h"

namespace NeneEngine::ECS
{

	struct MeshRendererComponent
	{
		PrimitiveType primitiveType = PrimitiveType::Triangle;
		bool visible = true;
		glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
		std::string meshPath;
		int meshPart = -1; // -1 imports the whole model.
		std::string texturePath;
		std::string shaderPath;
	};

} // namespace NeneEngine::ECS

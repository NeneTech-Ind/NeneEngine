#pragma once
#include <glm/vec3.hpp>
#include <string>

namespace NeneEngine
{
	struct EditorContext;
	bool CreateSceneAsset(EditorContext& context, const std::string& path, bool isTexture, const glm::vec3& position);
} // namespace NeneEngine

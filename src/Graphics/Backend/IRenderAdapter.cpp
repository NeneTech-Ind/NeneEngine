// IRenderAdapter.cpp

#include "Graphics/Backend/IRenderAdapter.h"

#include <array>
#include <utility>

namespace NeneEngine
{

	void IRenderAdapter::DrawDebugLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color,
	                                   const glm::mat4& viewProjection)
	{
		// The DebugLine primitive spans (-0.5, 0, 0)..(0.5, 0, 0), so only the X basis column and the translation
		// matter: X maps onto the segment vector and the origin onto its midpoint.
		glm::mat4 model(1.0f);
		model[0] = glm::vec4(to - from, 0.0f);
		model[3] = glm::vec4((from + to) * 0.5f, 1.0f);

		RenderItem item{};
		item.primitiveType = PrimitiveType::DebugLine;
		item.modelMatrix = model;
		item.viewProjectionMatrix = viewProjection;
		item.modelViewProjectionMatrix = viewProjection * model;
		item.tint = color;
		SubmitRenderItem(item);
	}

	void IRenderAdapter::DrawDebugAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color,
	                                   const glm::mat4& viewProjection)
	{
		const std::array<glm::vec3, 8> corners = {glm::vec3{min.x, min.y, min.z}, glm::vec3{max.x, min.y, min.z},
		                                          glm::vec3{max.x, max.y, min.z}, glm::vec3{min.x, max.y, min.z},
		                                          glm::vec3{min.x, min.y, max.z}, glm::vec3{max.x, min.y, max.z},
		                                          glm::vec3{max.x, max.y, max.z}, glm::vec3{min.x, max.y, max.z}};

		constexpr std::array<std::pair<int, int>, 12> edges = {
		    {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}}};

		for (const auto& [from, to] : edges) DrawDebugLine(corners[from], corners[to], color, viewProjection);
	}

} // namespace NeneEngine

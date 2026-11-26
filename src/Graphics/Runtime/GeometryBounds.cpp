// GeometryBounds.cpp

#include "Graphics/Runtime/GeometryBounds.h"

#include <cmath>

namespace NeneEngine
{
	std::optional<LocalAabb> GetPrimitiveLocalBounds(PrimitiveType primitive)
	{
		switch (primitive)
		{
		case PrimitiveType::Line:
		case PrimitiveType::DebugLine:
			return LocalAabb{{-0.5f, 0.0f, 0.0f}, {0.5f, 0.0f, 0.0f}};
		case PrimitiveType::Triangle:
		case PrimitiveType::Quad:
			return LocalAabb{{-0.5f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}};
		case PrimitiveType::Cube:
			return LocalAabb{glm::vec3(-0.4f), glm::vec3(0.4f)};
		}
		return std::nullopt;
	}

	std::optional<LocalAabb> ComputeLocalBounds(const MeshData& mesh)
	{
		if (mesh.vertices.empty()) return std::nullopt;

		const glm::vec3& first = mesh.vertices.front().position;
		LocalAabb bounds{first, first};
		for (const Vertex& vertex : mesh.vertices)
		{
			const glm::vec3& position = vertex.position;
			if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
				return std::nullopt;
			bounds.min = glm::min(bounds.min, position);
			bounds.max = glm::max(bounds.max, position);
		}
		return bounds;
	}
} // namespace NeneEngine

// MeshLoader.h

#pragma once

#include "Graphics/Runtime/RenderTypes.h"

#include <EASTL/vector.h>
#include <filesystem>
#include <string>

namespace NeneEngine
{

	struct MeshPart
	{
		MeshData data;
		std::filesystem::path diffuseTexturePath;
		std::string name;
		std::optional<LocalAabb> localBounds;
	};

	struct ImportedModel
	{
		eastl::vector<MeshPart> parts;
		std::optional<LocalAabb> localBounds;

		[[nodiscard]] std::optional<LocalAabb> GetLocalBounds(int part) const;
	};

	MeshData LoadMeshDataFromFile(const std::string& path);
	ImportedModel LoadMeshPartsFromFile(const std::string& path);
	Mesh LoadMeshFromFile(const std::string& path);

} // namespace NeneEngine

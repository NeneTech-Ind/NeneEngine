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
	};

	MeshData LoadMeshDataFromFile(const std::string& path);
	eastl::vector<MeshPart> LoadMeshPartsFromFile(const std::string& path);
	Mesh LoadMeshFromFile(const std::string& path);

} // namespace NeneEngine

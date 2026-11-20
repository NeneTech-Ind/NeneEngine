// AssetCatalog.cpp

#include "Editor/AssetCatalog.h"

#include "Core/PathResolver.h"
#include "Graphics/Runtime/SceneAssetResolver.h"

#include <algorithm>
#include <cctype>

namespace NeneEngine
{
	void AssetCatalog::Refresh()
	{
		m_entries.clear();
		m_error.clear();
		const auto root = ResolveFromExecutionRoots("assets");
		if (root.empty())
		{
			m_error = "Assets directory was not found";
			return;
		}
		try
		{
			for (auto it = std::filesystem::recursive_directory_iterator(
			         root, std::filesystem::directory_options::skip_permission_denied);
			     it != std::filesystem::recursive_directory_iterator{}; ++it)
			{
				const auto& entry = *it;
				if (it.depth() == 0 && entry.is_directory())
				{
					const auto name = entry.path().filename();
					if (name == "engine" || name == "readme" || name == "icons")
					{
						it.disable_recursion_pending();
						continue;
					}
				}
				if (!entry.is_regular_file()) continue;
				std::string extension = entry.path().extension().string();
				std::transform(extension.begin(), extension.end(), extension.begin(),
				               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				AssetKind kind;
				if (extension == ".obj" || extension == ".fbx" || extension == ".gltf" || extension == ".glb" ||
				    extension == ".dae" || extension == ".ply" || extension == ".stl")
					kind = AssetKind::Model;
				else if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
				         extension == ".tga" || extension == ".dds" || extension == ".tif" || extension == ".tiff")
					kind = AssetKind::Texture;
				else if (extension == ".shader")
					kind = AssetKind::Shader;
				else if (entry.path().filename().string().ends_with(".prefab.json"))
				{
					const auto relative = entry.path().lexically_relative(root / "prefabs");
					if (relative.empty() || relative.has_root_path() || *relative.begin() == "..") continue;
					kind = AssetKind::Prefab;
				}
				else
					continue;
				m_entries.push_back({PortableAssetPath(entry.path()), kind});
			}
			std::sort(m_entries.begin(), m_entries.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
		}
		catch (const std::exception& e)
		{
			m_error = e.what();
		}
	}
} // namespace NeneEngine

#pragma once

#include "ECS/Components/MeshRendererComponent.h"
#include "Graphics/Runtime/MeshRenderBinding.h"
#include "Graphics/Loaders/MeshLoader.h"
#include "Core/EASTLStdHash.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace NeneEngine
{
	class IRenderAdapter;

	class SceneAssetResolver final
	{
	  public:
		MeshRenderRuntimeBinding Resolve(const ECS::MeshRendererComponent& source, IRenderAdapter& renderer);
		GPUTexture Texture(const std::string& path, IRenderAdapter& renderer);
		GPUShaderProgram Shader(const std::string& path, IRenderAdapter& renderer);
		GPUMesh Mesh(const std::string& path, int part, IRenderAdapter& renderer);
		[[nodiscard]] const std::string& GetError() const { return m_error; }

	  private:
		std::unordered_map<std::string, GPUMesh> m_meshes;
		eastl::unordered_map<std::string, eastl::vector<MeshPart>> m_modelParts;
		std::unordered_map<std::string, GPUTexture> m_textures;
		std::unordered_map<std::string, GPUShaderProgram> m_shaders;
		std::string m_error;
	};

	std::string PortableAssetPath(const std::filesystem::path& path);
	std::filesystem::path ResolveAssetPath(const std::string& path);
}

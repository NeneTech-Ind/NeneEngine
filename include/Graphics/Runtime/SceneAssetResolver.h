#pragma once

#include "Core/EASTLStdHash.h"
#include "Core/ResourceLoadQueue.h"
#include "Graphics/Loaders/MeshLoader.h"
#include "Graphics/Runtime/MeshRenderBinding.h"

#include <filesystem>
#include <string>

namespace NeneEngine
{
	class IRenderAdapter;
	struct ModelAsset
	{
		ResourceLoadState state = ResourceLoadState::Loading;
		eastl::vector<MeshPart> parts;
		std::string error;
	};

	class SceneAssetResolver final
	{
	  public:
		explicit SceneAssetResolver(ResourceLoadQueue* loads = nullptr);
		MeshRenderRuntimeBinding Resolve(const std::string& meshPath, int meshPart, const std::string& texturePath,
		                                 const std::string& shaderPath, PrimitiveType primitive,
		                                 IRenderAdapter& renderer);
		eastl::shared_ptr<const ModelAsset> Model(const std::string& path);
		GPUTexture Texture(const std::string& path, IRenderAdapter& renderer);
		GPUShaderProgram Shader(const std::string& path, IRenderAdapter& renderer,
		                        JobPriority priority = JobPriority::Normal);
		GPUMesh Mesh(const std::string& path, int part, IRenderAdapter& renderer);
		[[nodiscard]] ResourceLoadState GetTextureState(const std::string& path, const IRenderAdapter& renderer) const;
		[[nodiscard]] ResourceLoadState GetShaderState(const std::string& path, const IRenderAdapter& renderer) const;
		[[nodiscard]] const std::string& GetError() const;
		void RetryFailed();

	  private:
		struct Impl;
		eastl::shared_ptr<Impl> m_impl;
	};

	std::string PortableAssetPath(const std::filesystem::path& path);
	std::filesystem::path ResolveAssetPath(const std::string& path);
} // namespace NeneEngine

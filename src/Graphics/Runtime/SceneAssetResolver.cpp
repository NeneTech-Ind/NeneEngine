#include "Graphics/Runtime/SceneAssetResolver.h"

#include "Core/PathResolver.h"
#include "Core/ResourceManager.h"
#include "Graphics/Backend/IRenderAdapter.h"
#include "Graphics/Loaders/MeshLoader.h"

namespace NeneEngine
{
	std::filesystem::path ResolveAssetPath(const std::string& path)
	{
		if (path.empty()) return {};
		const std::filesystem::path value(path);
		if (value.is_absolute()) return value;
		return ResolveFromExecutionRoots(value, true);
	}

	std::string PortableAssetPath(const std::filesystem::path& path)
	{
		if (path.empty()) return {};
		const auto root = ResolveFromExecutionRoots("assets");
		if (!root.empty())
		{
			const auto relative = std::filesystem::absolute(path).lexically_normal().lexically_relative(root.parent_path());
			if (!relative.empty() && *relative.begin() != "..") return relative.generic_string();
		}
		return std::filesystem::absolute(path).lexically_normal().generic_string();
	}

	namespace
	{
		std::string CacheKey(const std::string& path, const IRenderAdapter& renderer)
		{
			return std::to_string(renderer.GetResourceDomainKey()) + ":" + path;
		}
	}

	GPUMesh SceneAssetResolver::Mesh(const std::string& path, int part, IRenderAdapter& renderer)
	{
		if (path.empty()) return {};
		const std::string key = CacheKey(path, renderer) + ":" + std::to_string(part);
		if (const auto it = m_meshes.find(key); it != m_meshes.end()) return it->second;
		GPUMesh result;
		try
		{
			const auto resolved = ResolveAssetPath(path).string();
			if (path == "@quad")
			{
				MeshData quad;
				quad.vertices = {{{-0.5f, -0.5f, 0}, {0, 0, 1}, {0, 1}},
				                 {{0.5f, -0.5f, 0}, {0, 0, 1}, {1, 1}},
				                 {{0.5f, 0.5f, 0}, {0, 0, 1}, {1, 0}},
				                 {{-0.5f, 0.5f, 0}, {0, 0, 1}, {0, 0}}};
				quad.indices = {0, 1, 2, 0, 2, 3};
				result = renderer.UploadMesh(quad);
			}
			else if (part < 0)
			{
				auto resource = ResourceManager::GetInstance().Load<NeneEngine::Mesh>(resolved);
				if (resource) result = renderer.UploadMesh(resource->GetData().data);
			}
			else
			{
				if (m_modelParts.find(resolved) == m_modelParts.end()) m_modelParts.emplace(resolved, LoadMeshPartsFromFile(resolved));
				const auto& parts = m_modelParts.at(resolved);
				if (static_cast<size_t>(part) < parts.size()) result = renderer.UploadMesh(parts[part].data);
			}
		}
		catch (const std::exception& e) { NENE_LOG_ERROR("Model import failed: {}", e.what()); }
		if (!result.IsValid()) m_error = "Could not load model: " + path;
		m_meshes.emplace(key, result);
		return result;
	}

	GPUTexture SceneAssetResolver::Texture(const std::string& path, IRenderAdapter& renderer)
	{
		if (path.empty()) return {};
		const std::string key = CacheKey(path, renderer);
		if (const auto it = m_textures.find(key); it != m_textures.end()) return it->second;
		GPUTexture result;
		if (auto resource = ResourceManager::GetInstance().Load<TextureResource>(ResolveAssetPath(path).string()))
			result = renderer.CreateTexture2D(resource->GetData());
		if (!result.IsValid()) m_error = "Could not load texture: " + path;
		m_textures.emplace(key, result);
		return result;
	}

	GPUShaderProgram SceneAssetResolver::Shader(const std::string& path, IRenderAdapter& renderer)
	{
		if (path.empty()) return {};
		const std::string key = CacheKey(path, renderer);
		if (const auto it = m_shaders.find(key); it != m_shaders.end()) return it->second;
		GPUShaderProgram result;
		if (auto resource = ResourceManager::GetInstance().Load<ShaderProgramResource>(ResolveAssetPath(path).string()))
			result = renderer.CreateShaderProgram(resource->GetData());
		if (!result.IsValid()) m_error = "Could not compile shader: " + path;
		m_shaders.emplace(key, result);
		return result;
	}

	MeshRenderRuntimeBinding SceneAssetResolver::Resolve(const ECS::MeshRendererComponent& source, IRenderAdapter& renderer)
	{
		m_error.clear();
		MeshRenderRuntimeBinding result;
		result.meshId = Mesh(source.meshPath, source.meshPart, renderer).meshId;
		result.textureId = Texture(source.texturePath, renderer).textureId;
		result.shaderId = Shader(source.shaderPath, renderer).shaderId;
		return result;
	}
}

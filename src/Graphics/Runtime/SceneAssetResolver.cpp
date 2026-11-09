#include "Graphics/Runtime/SceneAssetResolver.h"

#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"
#include "Core/ResourceManager.h"
#include "Graphics/Backend/IRenderAdapter.h"
#include "Graphics/Loaders/MeshLoader.h"
#include <cmath>
#include <stdexcept>
#include <unordered_map>

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
			const auto relative =
			    std::filesystem::absolute(path).lexically_normal().lexically_relative(root.parent_path());
			if (!relative.empty() && *relative.begin() != "..") return relative.generic_string();
		}
		return std::filesystem::absolute(path).lexically_normal().generic_string();
	}

	namespace
	{
		std::string AssetPath(const std::string& path)
		{
			const auto resolved = ResolveAssetPath(path);
			return (resolved.empty() ? std::filesystem::absolute(path) : resolved).lexically_normal().generic_string();
		}

		std::string CacheKey(const std::string& path, const IRenderAdapter& renderer)
		{
			return std::to_string(renderer.GetResourceDomainKey()) + ":" + path;
		}

		GPUMesh CreateBuiltinMesh(const std::string& path, IRenderAdapter& renderer)
		{
			MeshData quad;
			quad.vertices = {{{-0.5f, -0.5f, 0}, {0, 0, 1}, {0, 1}},
			                 {{0.5f, -0.5f, 0}, {0, 0, 1}, {1, 1}},
			                 {{0.5f, 0.5f, 0}, {0, 0, 1}, {1, 0}},
			                 {{-0.5f, 0.5f, 0}, {0, 0, 1}, {0, 0}}};
			quad.indices = {0, 1, 2, 0, 2, 3};
			if (path == "@triangle")
			{
				quad.vertices = {{{-0.5f, -0.5f, 0}, {0, 0, 1}, {0, 1}},
				                 {{0, 0.5f, 0}, {0, 0, 1}, {0.5f, 0}},
				                 {{0.5f, -0.5f, 0}, {0, 0, 1}, {1, 1}}};
				quad.indices = {0, 1, 2};
			}
			else if (path == "@cube")
			{
				quad = {};
				const glm::vec3 normals[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
				for (const auto& normal : normals)
				{
					const glm::vec3 tangent = std::abs(normal.y) > 0.5f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
					const glm::vec3 bitangent = glm::cross(normal, tangent);
					const uint32_t base = static_cast<uint32_t>(quad.vertices.size());
					quad.vertices.push_back({0.4f * (normal - tangent - bitangent), normal, {0, 1}});
					quad.vertices.push_back({0.4f * (normal + tangent - bitangent), normal, {1, 1}});
					quad.vertices.push_back({0.4f * (normal + tangent + bitangent), normal, {1, 0}});
					quad.vertices.push_back({0.4f * (normal - tangent + bitangent), normal, {0, 0}});
					for (uint32_t index : {0u, 1u, 2u, 0u, 2u, 3u}) quad.indices.push_back(base + index);
				}
			}

			return renderer.UploadMesh(quad);
		}

		template <typename T> struct AssetEntry
		{
			ResourceLoadState state = ResourceLoadState::Loading;
			T data{};
			std::string error;
		};

		template <typename T> void RequireValid(const T& value)
		{
			if (!value.IsValid()) throw std::runtime_error("Could not create GPU resource");
		}
	} // namespace

	struct SceneAssetResolver::Impl
	{
		ResourceLoadQueue* loads = nullptr;
		std::unordered_map<std::string, eastl::shared_ptr<ModelAsset>> models;
		std::unordered_map<std::string, eastl::shared_ptr<AssetEntry<GPUMesh>>> meshes;
		std::unordered_map<std::string, eastl::shared_ptr<AssetEntry<GPUTexture>>> textures;
		std::unordered_map<std::string, eastl::shared_ptr<AssetEntry<GPUShaderProgram>>> shaders;
		std::unordered_map<uintptr_t, GPUTexture> placeholders;
		std::string error;
	};

	SceneAssetResolver::SceneAssetResolver(ResourceLoadQueue* loads) : m_impl(eastl::make_shared<Impl>())
	{
		m_impl->loads = loads;
	}

	eastl::shared_ptr<const ModelAsset> SceneAssetResolver::Model(const std::string& path)
	{
		const auto resolved = AssetPath(path);
		if (const auto it = m_impl->models.find(resolved); it != m_impl->models.end()) return it->second;
		auto asset = eastl::make_shared<ModelAsset>();
		m_impl->models.emplace(resolved, asset);
		if (!m_impl->loads) throw std::logic_error("Asset resolver has no resource loading queue");
		const auto state = m_impl;
		const auto fail = [asset, state](const std::string& error)
		{
			asset->state = ResourceLoadState::Failed;
			asset->error = error;
			state->error = error;
			NENE_LOG_ERROR("Model loading failed: {}", error);
		};
		if (!m_impl->loads->Request(
		        resolved,
		        [asset, resolved]() -> ResourceLoadQueue::Completion
		        {
			        const auto resource = ResourceManager::GetInstance().Load<eastl::vector<MeshPart>>(resolved);
			        if (!resource) throw std::runtime_error("Could not read model: " + resolved);
			        auto parts = resource->GetData();
			        return [asset, parts = std::move(parts)]() mutable
			        {
				        asset->parts = std::move(parts);
				        asset->state = ResourceLoadState::Ready;
			        };
		        },
		        fail))
			fail("Resource loader is stopping");
		return asset;
	}

	GPUMesh SceneAssetResolver::Mesh(const std::string& path, int part, IRenderAdapter& renderer)
	{
		if (path.empty()) return {};
		const bool builtin = path == "@quad" || path == "@triangle" || path == "@cube";
		const auto resolved = builtin ? path : AssetPath(path);
		const auto key = CacheKey(resolved, renderer) + ":" + std::to_string(part);
		if (const auto it = m_impl->meshes.find(key); it != m_impl->meshes.end())
		{
			if (it->second->state == ResourceLoadState::Ready) return it->second->data;
			if (it->second->state == ResourceLoadState::Failed) return {};
			return Mesh("@cube", -1, renderer);
		}
		if (builtin)
		{
			auto entry = eastl::make_shared<AssetEntry<GPUMesh>>();
			entry->data = CreateBuiltinMesh(path, renderer);
			entry->state = entry->data.IsValid() ? ResourceLoadState::Ready : ResourceLoadState::Failed;
			m_impl->meshes.emplace(key, entry);
			return entry->data;
		}
		const auto model = Model(resolved);
		if (model->state == ResourceLoadState::Failed) return {};
		if (model->state != ResourceLoadState::Ready) return Mesh("@cube", -1, renderer);
		auto entry = eastl::make_shared<AssetEntry<GPUMesh>>();
		m_impl->meshes.emplace(key, entry);
		const auto state = m_impl;
		const auto fail = [entry, state](const std::string& error)
		{
			entry->state = ResourceLoadState::Failed;
			entry->error = error;
			state->error = error;
		};
		if (!m_impl->loads->Request(
		        resolved,
		        [model, part, entry, &renderer]() -> ResourceLoadQueue::Completion
		        {
			        if (part >= 0)
			        {
				        if (static_cast<size_t>(part) >= model->parts.size())
					        throw std::runtime_error("Model part does not exist");
				        return [model, part, entry, &renderer]()
				        {
					        entry->data = renderer.UploadMesh(model->parts[part].data);
					        RequireValid(entry->data);
					        entry->state = ResourceLoadState::Ready;
				        };
			        }
			        MeshData merged;
			        for (const auto& mesh : model->parts)
			        {
				        const auto base = static_cast<uint32_t>(merged.vertices.size());
				        merged.vertices.insert(merged.vertices.end(), mesh.data.vertices.begin(),
				                               mesh.data.vertices.end());
				        for (uint32_t index : mesh.data.indices) merged.indices.push_back(base + index);
			        }
			        return [entry, merged = std::move(merged), &renderer]()
			        {
				        entry->data = renderer.UploadMesh(merged);
				        RequireValid(entry->data);
				        entry->state = ResourceLoadState::Ready;
			        };
		        },
		        fail))
			fail("Resource loader is stopping");
		return Mesh("@cube", -1, renderer);
	}

	GPUTexture SceneAssetResolver::Texture(const std::string& path, IRenderAdapter& renderer)
	{
		if (path.empty()) return {};
		const auto resolved = AssetPath(path);
		const auto key = CacheKey(resolved, renderer);
		auto it = m_impl->textures.find(key);
		if (it == m_impl->textures.end())
		{
			auto entry = eastl::make_shared<AssetEntry<GPUTexture>>();
			m_impl->textures.emplace(key, entry);
			const auto state = m_impl;
			const auto fail = [entry, state](const std::string& error)
			{
				entry->state = ResourceLoadState::Failed;
				entry->error = error;
				state->error = error;
				NENE_LOG_ERROR("Texture loading failed: {}", error);
			};
			if (!m_impl->loads) throw std::logic_error("Asset resolver has no resource loading queue");
			if (!m_impl->loads->Request(
			        resolved,
			        [entry, resolved, &renderer]() -> ResourceLoadQueue::Completion
			        {
				        auto resource = ResourceManager::GetInstance().Load<TextureResource>(resolved);
				        if (!resource) throw std::runtime_error("Could not read texture: " + resolved);
				        auto prepared = renderer.PrepareTexture2D(resource->GetData());
				        return [entry, resource, prepared, &renderer]()
				        {
					        entry->data = renderer.CreateTexture2D(resource->GetData(), *prepared);
					        RequireValid(entry->data);
					        entry->state = ResourceLoadState::Ready;
				        };
			        },
			        fail))
				fail("Resource loader is stopping");
			it = m_impl->textures.find(key);
		}
		if (it->second->state == ResourceLoadState::Ready) return it->second->data;
		if (it->second->state == ResourceLoadState::Failed) return {};
		auto& placeholder = m_impl->placeholders[renderer.GetResourceDomainKey()];
		if (!placeholder.IsValid()) placeholder = renderer.CreatePlaceholderTexture();
		return placeholder;
	}

	GPUShaderProgram SceneAssetResolver::Shader(const std::string& path, IRenderAdapter& renderer, JobPriority priority)
	{
		if (path.empty()) return {};
		const auto resolved = AssetPath(path);
		const auto key = CacheKey(resolved, renderer);
		if (const auto it = m_impl->shaders.find(key); it != m_impl->shaders.end()) return it->second->data;
		auto entry = eastl::make_shared<AssetEntry<GPUShaderProgram>>();
		m_impl->shaders.emplace(key, entry);
		const auto state = m_impl;
		const auto fail = [entry, state, resolved](const std::string& error)
		{
			entry->state = ResourceLoadState::Failed;
			entry->error = error;
			state->error = error;
			ResourceManager::GetInstance().Invalidate<ShaderProgramResource>(resolved);
			NENE_LOG_ERROR("Shader loading failed: {}", error);
		};
		if (!m_impl->loads) throw std::logic_error("Asset resolver has no resource loading queue");
		if (!m_impl->loads->Request(
		        resolved,
		        [entry, resolved, &renderer]() -> ResourceLoadQueue::Completion
		        {
			        auto resource = ResourceManager::GetInstance().Load<ShaderProgramResource>(resolved);
			        if (!resource) throw std::runtime_error("Could not read shader: " + resolved);
			        return [entry, resource, &renderer]()
			        {
				        entry->data = renderer.CreateShaderProgram(resource->GetData());
				        RequireValid(entry->data);
				        entry->state = ResourceLoadState::Ready;
			        };
		        },
		        fail, priority))
			fail("Resource loader is stopping");
		return {};
	}

	ResourceLoadState SceneAssetResolver::GetTextureState(const std::string& path, const IRenderAdapter& renderer) const
	{
		const auto it = m_impl->textures.find(CacheKey(AssetPath(path), renderer));
		return it == m_impl->textures.end() ? ResourceLoadState::Loading : it->second->state;
	}

	ResourceLoadState SceneAssetResolver::GetShaderState(const std::string& path, const IRenderAdapter& renderer) const
	{
		const auto it = m_impl->shaders.find(CacheKey(AssetPath(path), renderer));
		return it == m_impl->shaders.end() ? ResourceLoadState::Loading : it->second->state;
	}

	const std::string& SceneAssetResolver::GetError() const
	{
		return m_impl->error;
	}

	void SceneAssetResolver::RetryFailed()
	{
		const auto failed = [](const auto& entry) { return entry.second->state == ResourceLoadState::Failed; };
		std::erase_if(m_impl->models, failed);
		std::erase_if(m_impl->meshes, failed);
		std::erase_if(m_impl->textures, failed);
		std::erase_if(m_impl->shaders, failed);
		m_impl->error.clear();
	}

	MeshRenderRuntimeBinding SceneAssetResolver::Resolve(const std::string& sourcePath, int meshPart,
	                                                     const std::string& texturePath, const std::string& shaderPath,
	                                                     PrimitiveType primitive, IRenderAdapter& renderer)
	{
		MeshRenderRuntimeBinding result;
		std::string meshPath = sourcePath;
		if (meshPath.empty() && (!texturePath.empty() || !shaderPath.empty()))
		{
			switch (primitive)
			{
			case PrimitiveType::Cube:
				meshPath = "@cube";
				break;
			case PrimitiveType::Quad:
				meshPath = "@quad";
				break;
			case PrimitiveType::Triangle:
				meshPath = "@triangle";
				break;
			default:
				break;
			}
		}
		result.meshId = Mesh(meshPath, meshPart, renderer).meshId;
		result.textureId = Texture(texturePath, renderer).textureId;
		result.shaderId = Shader(shaderPath, renderer).shaderId;
		return result;
	}
} // namespace NeneEngine

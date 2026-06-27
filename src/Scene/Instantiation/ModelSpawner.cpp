#include "Scene/Instantiation/ModelSpawner.h"

#include "Core/CustomLogger.h"
#include "Core/PathResolver.h"
#include "Core/ResourceManager.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Graphics/Backend/IRenderAdapter.h"
#include "Graphics/Loaders/MeshLoader.h"
#include "Scene/Instantiation/ModelInstanceConfig.h"
#include "Scene/Instantiation/ModelSpawnManifest.h"
#include "Graphics/Runtime/MeshRenderBinding.h"

#include <fstream>
#include <glm/gtc/quaternion.hpp>
#include <sstream>
#include <vector>

namespace NeneEngine
{
	namespace
	{
		std::filesystem::path FindDiffuseTextureFromObjMaterial(const std::filesystem::path& objPath)
		{
			std::ifstream objFile(objPath);
			if (!objFile) return {};

			std::string line;
			std::filesystem::path materialPath;
			while (std::getline(objFile, line))
			{
				std::istringstream stream(line);
				std::string command;
				stream >> command;
				if (command != "mtllib") continue;

				std::string materialFile;
				std::getline(stream >> std::ws, materialFile);
				if (!materialFile.empty())
				{
					materialPath = objPath.parent_path() / materialFile;
					break;
				}
			}

			if (materialPath.empty()) return {};

			std::ifstream materialFile(materialPath);
			if (!materialFile) return {};

			while (std::getline(materialFile, line))
			{
				std::istringstream stream(line);
				std::string command;
				stream >> command;
				if (command != "map_Kd") continue;

				std::string textureFile;
				std::getline(stream >> std::ws, textureFile);
				if (!textureFile.empty()) return materialPath.parent_path() / textureFile;
			}

			return {};
		}

		TextureId CreateTexture(IRenderAdapter& renderer, const std::filesystem::path& texturePath)
		{
			if (texturePath.empty() || !std::filesystem::exists(texturePath)) return {};

			auto textureResource = ResourceManager::GetInstance().Load<TextureResource>(texturePath.string());
			if (textureResource == nullptr) return {};

			const GPUTexture gpuTexture = renderer.CreateTexture2D(textureResource->GetData());
			return gpuTexture.textureId;
		}

		uintptr_t GetRendererKey(const IRenderAdapter& renderer)
		{
			return reinterpret_cast<uintptr_t>(&renderer);
		}

		const GPUMesh* FindUploadedMeshForRenderer(const Mesh& mesh, const IRenderAdapter& renderer)
		{
			const auto it = mesh.gpuMeshesByRenderer.find(GetRendererKey(renderer));
			return it != mesh.gpuMeshesByRenderer.end() ? &it->second : nullptr;
		}

		ECS::Entity CreateUploadedModelEntity(ECS::World& world, std::string_view name, const glm::vec3& position,
		                                      const glm::vec3& scale, const glm::vec3& worldOffset = {0.0f, 0.0f, 0.0f},
		                                      const glm::vec3& rotationOffsetDegrees = {0.0f, 0.0f, 0.0f},
		                                      bool visible = true)
		{
			const ECS::Entity modelEntity = world.CreateEntity(std::string(name));
			auto& modelTransform = world.AddComponent<ECS::TransformComponent>(modelEntity);
			modelTransform.position = position + worldOffset;
			modelTransform.scale = scale;
			modelTransform.rotation = glm::quat(glm::radians(rotationOffsetDegrees));

			auto& modelRenderer = world.AddComponent<ECS::MeshRendererComponent>(modelEntity);
			modelRenderer.tint = {1.0f, 1.0f, 1.0f, 1.0f};
			modelRenderer.visible = visible;

			return modelEntity;
		}

		void BindUploadedModelEntity(ECS::World& world, ECS::Entity entity, const IRenderAdapter& renderer,
		                             const GPUMesh& gpuMesh, ShaderId shaderId, TextureId textureId)
		{
			MeshRenderRuntimeBinding runtimeBinding{};
			runtimeBinding.meshId = gpuMesh.meshId;
			runtimeBinding.textureId = textureId;
			if (shaderId.IsValid() && textureId.IsValid()) runtimeBinding.shaderId = shaderId;
			BindMeshRenderRuntime(world, entity, runtimeBinding, &renderer);
		}

		std::filesystem::path ResolveOptionalAssetPath(const std::filesystem::path& manifestPath,
		                                               const std::filesystem::path& path)
		{
			if (path.empty()) return {};
			if (path.is_absolute()) return std::filesystem::exists(path) ? path : std::filesystem::path{};

			const std::filesystem::path manifestDirectory = manifestPath.parent_path();
			const std::filesystem::path relativeToManifest = manifestDirectory / path;
			if (std::filesystem::exists(relativeToManifest)) return relativeToManifest;
			if (std::filesystem::exists(path)) return path;

			if (const auto resolvedFromCurrent = ResolveFromAncestors(std::filesystem::current_path(), path);
			    !resolvedFromCurrent.empty())
				return resolvedFromCurrent;

			return ResolveFromAncestors(manifestDirectory, path);
		}

	} // namespace

	ShaderId CreateTexturedMeshShader(IRenderAdapter& renderer, const std::filesystem::path& shaderPath)
	{
		if (shaderPath.empty() || !std::filesystem::exists(shaderPath)) return {};

		auto shaderResource = ResourceManager::GetInstance().Load<ShaderProgramResource>(shaderPath.string());
		if (shaderResource == nullptr) return {};

		const GPUShaderProgram gpuShader = renderer.CreateShaderProgram(shaderResource->GetData());
		return gpuShader.shaderId;
	}

	void SpawnModelsFromManifest(ECS::World& world, std::span<IRenderAdapter* const> renderers,
	                             const std::filesystem::path& shaderPath, const std::filesystem::path& manifestPath)
	{
		if (renderers.empty()) return;

		std::vector<IRenderAdapter*> validRenderers;
		validRenderers.reserve(renderers.size());
		std::vector<ShaderId> shaderIds;
		shaderIds.reserve(renderers.size());
		for (IRenderAdapter* renderer : renderers)
		{
			if (renderer == nullptr) continue;
			validRenderers.push_back(renderer);
			shaderIds.push_back(CreateTexturedMeshShader(*renderer, shaderPath));
		}

		if (validRenderers.empty()) return;

		const ModelSpawnManifestConfig manifest = LoadModelSpawnManifest(manifestPath);

		for (const auto& modelEntry : manifest.models)
		{
			const auto meshPath = ResolveOptionalAssetPath(manifestPath, modelEntry.meshPath);
			if (meshPath.empty())
			{
				NENE_LOG_WARN("Model entry '{}' mesh '{}' was not found", modelEntry.entityName,
				              modelEntry.meshPath.string());
				continue;
			}

			const auto instanceConfigPath = ResolveOptionalAssetPath(manifestPath, modelEntry.instanceConfigPath);
			const ModelInstanceConfig modelConfig = LoadModelInstanceConfig(instanceConfigPath);

			if (!modelEntry.splitByMeshParts)
			{
				if (auto meshResource = ResourceManager::GetInstance().Load<Mesh>(meshPath.string());
				    meshResource != nullptr)
				{
					Mesh& mesh = meshResource->GetData();
					auto texturePath = FindDiffuseTextureFromObjMaterial(meshPath);
					if (!texturePath.empty() && !std::filesystem::exists(texturePath)) texturePath.clear();
					const ECS::Entity modelEntity =
					    CreateUploadedModelEntity(world, modelEntry.entityName, modelConfig.position, modelConfig.scale);
					bool boundForAnyRenderer = false;

					for (size_t rendererIndex = 0; rendererIndex < validRenderers.size(); ++rendererIndex)
					{
						IRenderAdapter& renderer = *validRenderers[rendererIndex];
						if (const GPUMesh* uploadedMesh = FindUploadedMeshForRenderer(mesh, renderer);
						    uploadedMesh == nullptr || !uploadedMesh->IsValid())
						{
							const GPUMesh gpuMesh = renderer.UploadMesh(mesh.data);
							if (gpuMesh.IsValid()) mesh.gpuMeshesByRenderer[GetRendererKey(renderer)] = gpuMesh;
						}

						const GPUMesh* uploadedMesh = FindUploadedMeshForRenderer(mesh, renderer);
						if (uploadedMesh == nullptr || !uploadedMesh->IsValid()) continue;

						const TextureId textureId = CreateTexture(renderer, texturePath);
						BindUploadedModelEntity(world, modelEntity, renderer, *uploadedMesh, shaderIds[rendererIndex],
						                        textureId);
						boundForAnyRenderer = true;

						NENE_LOG_INFO("Assigned uploaded meshId={} to standalone entity '{}' for renderer {}",
						              uploadedMesh->meshId.value, modelEntry.entityName, rendererIndex);
					}

					if (!boundForAnyRenderer) world.DestroyEntity(modelEntity);
				}

				continue;
			}

			const auto modelDirectory = meshPath.parent_path();
			const std::vector<MeshPart> meshParts = LoadMeshPartsFromFile(meshPath.string());
			for (size_t meshPartIndex = 0; meshPartIndex < meshParts.size(); ++meshPartIndex)
			{
				const MeshPart& meshPart = meshParts[meshPartIndex];
				const ModelPartOverrideConfig* overrideConfig = FindPartOverride(modelConfig, meshPart.name);
				const std::filesystem::path texturePath =
				    overrideConfig != nullptr && !overrideConfig->textureOverride.empty()
				        ? (overrideConfig->textureOverride.is_absolute()
				               ? overrideConfig->textureOverride
				               : modelDirectory / overrideConfig->textureOverride)
				        : meshPart.diffuseTexturePath;
				const glm::vec3 worldOffset =
				    overrideConfig != nullptr ? overrideConfig->positionOffset : glm::vec3{0.0f, 0.0f, 0.0f};
				const glm::vec3 rotationOffsetDegrees =
				    overrideConfig != nullptr ? overrideConfig->rotationOffsetDegrees : glm::vec3{0.0f, 0.0f, 0.0f};
				const glm::vec3 scale =
				    overrideConfig != nullptr ? modelConfig.scale * overrideConfig->scaleMultiplier : modelConfig.scale;
				const bool visible = overrideConfig != nullptr ? overrideConfig->visible : true;
				const ECS::Entity modelEntity = CreateUploadedModelEntity(
				    world, modelEntry.entityName + "_" + std::to_string(meshPartIndex), modelConfig.position, scale,
				    worldOffset, rotationOffsetDegrees, visible);
				bool boundForAnyRenderer = false;

				for (size_t rendererIndex = 0; rendererIndex < validRenderers.size(); ++rendererIndex)
				{
					IRenderAdapter& renderer = *validRenderers[rendererIndex];
					const GPUMesh gpuMesh = renderer.UploadMesh(meshPart.data);
					if (!gpuMesh.IsValid()) continue;

					const TextureId textureId = CreateTexture(renderer, texturePath);
					BindUploadedModelEntity(world, modelEntity, renderer, gpuMesh, shaderIds[rendererIndex], textureId);
					boundForAnyRenderer = true;

					NENE_LOG_INFO(
					    "Uploaded mesh part '{}' for '{}' as meshId={} (vertices={}, indices={}, texture='{}', renderer={})",
					    meshPart.name, modelEntry.entityName, gpuMesh.meshId.value, gpuMesh.vertexCount,
					    gpuMesh.indexCount, texturePath.string(), rendererIndex);
				}

				if (!boundForAnyRenderer) world.DestroyEntity(modelEntity);
			}
		}
	}

} // namespace NeneEngine

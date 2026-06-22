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

		ECS::Entity CreateUploadedModelEntity(ECS::World& world, std::string_view name, const glm::vec3& position,
		                                      const glm::vec3& scale, const GPUMesh& gpuMesh, ShaderId shaderId,
		                                      TextureId textureId, const glm::vec3& worldOffset = {0.0f, 0.0f, 0.0f},
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

			MeshRenderRuntimeBinding runtimeBinding{};
			runtimeBinding.meshId = gpuMesh.meshId;
			runtimeBinding.textureId = textureId;
			if (shaderId.IsValid() && textureId.IsValid()) runtimeBinding.shaderId = shaderId;
			BindMeshRenderRuntime(world, modelEntity, runtimeBinding);

			return modelEntity;
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

	void SpawnModelsFromManifest(ECS::World& world, IRenderAdapter& renderer, ShaderId shaderId,
	                             const std::filesystem::path& manifestPath)
	{
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
					if (!mesh.gpuMesh.has_value() || !mesh.gpuMesh->IsValid())
					{
						const GPUMesh gpuMesh = renderer.UploadMesh(mesh.data);
						if (gpuMesh.IsValid()) mesh.gpuMesh = gpuMesh;
					}

					if (mesh.gpuMesh.has_value() && mesh.gpuMesh->IsValid())
					{
						auto texturePath = FindDiffuseTextureFromObjMaterial(meshPath);
						if (!texturePath.empty() && !std::filesystem::exists(texturePath)) texturePath.clear();
						const TextureId textureId = CreateTexture(renderer, texturePath);

						CreateUploadedModelEntity(world, modelEntry.entityName, modelConfig.position, modelConfig.scale,
						                          *mesh.gpuMesh, shaderId, textureId);

						NENE_LOG_INFO("Assigned uploaded meshId={} to standalone entity '{}'",
						              mesh.gpuMesh->meshId.value, modelEntry.entityName);
					}
				}

				continue;
			}

			const auto modelDirectory = meshPath.parent_path();
			const std::vector<MeshPart> meshParts = LoadMeshPartsFromFile(meshPath.string());
			for (size_t meshPartIndex = 0; meshPartIndex < meshParts.size(); ++meshPartIndex)
			{
				const MeshPart& meshPart = meshParts[meshPartIndex];
				const GPUMesh gpuMesh = renderer.UploadMesh(meshPart.data);
				if (!gpuMesh.IsValid()) continue;

				const ModelPartOverrideConfig* overrideConfig = FindPartOverride(modelConfig, meshPart.name);
				const std::filesystem::path texturePath =
				    overrideConfig != nullptr && !overrideConfig->textureOverride.empty()
				        ? (overrideConfig->textureOverride.is_absolute()
				               ? overrideConfig->textureOverride
				               : modelDirectory / overrideConfig->textureOverride)
				        : meshPart.diffuseTexturePath;
				const TextureId textureId = CreateTexture(renderer, texturePath);
				const glm::vec3 worldOffset =
				    overrideConfig != nullptr ? overrideConfig->positionOffset : glm::vec3{0.0f, 0.0f, 0.0f};
				const glm::vec3 rotationOffsetDegrees =
				    overrideConfig != nullptr ? overrideConfig->rotationOffsetDegrees : glm::vec3{0.0f, 0.0f, 0.0f};
				const glm::vec3 scale =
				    overrideConfig != nullptr ? modelConfig.scale * overrideConfig->scaleMultiplier : modelConfig.scale;
				const bool visible = overrideConfig != nullptr ? overrideConfig->visible : true;

				CreateUploadedModelEntity(world, modelEntry.entityName + "_" + std::to_string(meshPartIndex),
				                          modelConfig.position, scale, gpuMesh, shaderId, textureId, worldOffset,
				                          rotationOffsetDegrees, visible);

				NENE_LOG_INFO("Uploaded mesh part '{}' for '{}' as meshId={} (vertices={}, indices={}, texture='{}')",
				              meshPart.name, modelEntry.entityName, gpuMesh.meshId.value, gpuMesh.vertexCount,
				              gpuMesh.indexCount, texturePath.string());
			}
		}
	}

} // namespace NeneEngine

// ModelSpawner.cpp

#include "Scene/Instantiation/ModelSpawner.h"

#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"
#include "Core/ResourceLoadQueue.h"
#include "Core/ResourceManager.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Events/SceneRestoredEvent.h"
#include "ECS/World.h"
#include "Graphics/Loaders/MeshLoader.h"
#include "Graphics/Runtime/SceneAssetResolver.h"
#include "Scene/Instantiation/ModelInstanceConfig.h"
#include "Scene/Instantiation/ModelSpawnManifest.h"

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

		struct PreparedModelEntity
		{
			std::string name;
			ECS::TransformComponent transform;
			ECS::MeshRendererComponent renderer;
		};
	} // namespace

	void SpawnModelsFromManifest(ECS::World& world, ResourceLoadQueue& loads, const std::filesystem::path& shaderPath,
	                             const std::filesystem::path& manifestPath)
	{
		auto cancelled = eastl::make_shared<bool>(false);
		world.GetEventBus().Subscribe<ECS::SceneRestoredEvent>([cancelled](const auto&) { *cancelled = true; });
		loads.Request(
		    "Model Manifest",
		    [&world, cancelled, shaderPath, manifestPath]() -> ResourceLoadQueue::Completion
		    {
			    eastl::vector<PreparedModelEntity> entities;
			    const auto manifest = LoadModelSpawnManifest(manifestPath);
			    for (const auto& model : manifest.models)
			    {
				    const auto path = ResolveOptionalAssetPath(manifestPath, model.meshPath);
				    if (path.empty()) continue;
				    const auto config =
				        LoadModelInstanceConfig(ResolveOptionalAssetPath(manifestPath, model.instanceConfigPath));
				    const auto resource =
				        ResourceManager::GetInstance().Load<eastl::vector<MeshPart>>(path.generic_string());
				    if (!resource) continue;
				    const auto& parts = resource->GetData();
				    const size_t count = model.splitByMeshParts ? parts.size() : 1;
				    for (size_t index = 0; index < count; ++index)
				    {
					    PreparedModelEntity entity;
					    entity.name = model.entityName + (model.splitByMeshParts ? "_" + std::to_string(index) : "");
					    entity.transform.position = config.position;
					    entity.transform.scale = config.scale;
					    entity.renderer.meshPath = PortableAssetPath(path);
					    entity.renderer.meshPart = model.splitByMeshParts ? static_cast<int>(index) : -1;
					    entity.renderer.shaderPath = PortableAssetPath(shaderPath);
					    auto texturePath = model.splitByMeshParts ? parts[index].diffuseTexturePath
					                                              : FindDiffuseTextureFromObjMaterial(path);
					    if (model.splitByMeshParts)
					    {
						    if (const auto* part = FindPartOverride(config, parts[index].name))
						    {
							    entity.transform.position += part->positionOffset;
							    entity.transform.scale *= part->scaleMultiplier;
							    entity.transform.rotation = glm::quat(glm::radians(part->rotationOffsetDegrees));
							    entity.renderer.visible = part->visible;
							    if (!part->textureOverride.empty())
								    texturePath = path.parent_path() / part->textureOverride;
						    }
					    }
					    entity.renderer.texturePath = PortableAssetPath(texturePath);
					    entities.push_back(std::move(entity));
				    }
			    }
			    return [&world, cancelled, entities = std::move(entities)]()
			    {
				    if (*cancelled) return;
				    for (const auto& source : entities)
				    {
					    const auto entity = world.CreateEntity(source.name);
					    world.AddComponent<ECS::TransformComponent>(entity) = source.transform;
					    world.AddComponent<ECS::MeshRendererComponent>(entity) = source.renderer;
				    }
			    };
		    },
		    [](const std::string& error) { NENE_LOG_ERROR("Model manifest failed: {}", error); });
	}
} // namespace NeneEngine

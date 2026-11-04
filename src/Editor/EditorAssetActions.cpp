#include "Editor/EditorAssetActions.h"
#include "Core/NeneLogger.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorHistory.h"
#include "Graphics/Loaders/MeshLoader.h"
#include "Scene/Instantiation/ModelInstanceConfig.h"
#include <vector>

namespace NeneEngine
{
	namespace
	{
		struct CreatedEntity
		{
			ECS::Entity entity;
			ECS::TagComponent tag;
			ECS::TransformComponent transform;
			ECS::MeshRendererComponent mesh;
			ECS::HierarchyComponent hierarchy;
			bool hasMesh = true;
		};

		class CreateAssetCommand final : public IEditorCommand
		{
		  public:
			explicit CreateAssetCommand(std::vector<CreatedEntity> entities) : m_entities(std::move(entities)) {}
			const char* GetName() const override { return "Create Asset"; }
			void Execute(ECS::World& world) override
			{
				for (const auto& entry : m_entities)
				{
					if (!world.GetRegistry().valid(entry.entity))
					{
						const auto recreated = world.GetRegistry().create(entry.entity);
						if (recreated != entry.entity)
						{
							world.GetRegistry().destroy(recreated);
							return;
						}
					}
					world.GetRegistry().emplace_or_replace<ECS::TagComponent>(entry.entity, entry.tag);
					world.GetRegistry().emplace_or_replace<ECS::TransformComponent>(entry.entity, entry.transform);
					world.GetRegistry().emplace_or_replace<ECS::HierarchyComponent>(entry.entity, entry.hierarchy);
					if (entry.hasMesh)
						world.GetRegistry().emplace_or_replace<ECS::MeshRendererComponent>(entry.entity, entry.mesh);
				}
			}
			void Undo(ECS::World& world) override
			{
				for (auto it = m_entities.rbegin(); it != m_entities.rend(); ++it)
					if (world.GetRegistry().valid(it->entity)) world.DestroyEntity(it->entity);
			}

		  private:
			std::vector<CreatedEntity> m_entities;
		};
	} // namespace

	bool CreateSceneAsset(EditorContext& context, const std::string& path, bool isTexture, const glm::vec3& position)
	{
		if (context.mode != EditorMode::Edit || context.renderer == nullptr) return false;
		std::vector<CreatedEntity> created;
		try
		{
			const auto resolved = ResolveAssetPath(path);
			const auto parts = isTexture ? eastl::vector<MeshPart>{} : LoadMeshPartsFromFile(resolved.string());
			if (!isTexture && parts.empty()) throw std::runtime_error("Model has no mesh parts");
			if (isTexture && !context.assetResolver.Texture(path, *context.renderer).IsValid())
				throw std::runtime_error("Texture could not be loaded");
			const ModelInstanceConfig config =
			    isTexture ? ModelInstanceConfig{}
			              : LoadModelInstanceConfig(resolved.parent_path() / "model.instance.json");
			const auto root = context.world.CreateEntity(resolved.stem().string());
			CreatedEntity rootEntry{root, ECS::TagComponent(resolved.stem().string()), {}, {}, {}, isTexture};
			rootEntry.transform.position = position;
			if (isTexture)
			{
				rootEntry.mesh.meshPath = "@quad";
				rootEntry.mesh.texturePath = path;
				rootEntry.mesh.shaderPath = "assets/shaders/textured_mesh.shader";
			}
			created.push_back(rootEntry);
			for (size_t i = 0; i < parts.size(); ++i)
			{
				const auto child = context.world.CreateEntity(parts[i].name);
				CreatedEntity entry{child, ECS::TagComponent(parts[i].name), {}, {}, {}, true};
				entry.hierarchy.parent = root;
				entry.transform.scale = config.scale;
				entry.mesh.meshPath = path;
				entry.mesh.meshPart = static_cast<int>(i);
				entry.mesh.texturePath = PortableAssetPath(parts[i].diffuseTexturePath);
				entry.mesh.shaderPath = "assets/shaders/textured_mesh.shader";
				if (const auto* partOverride = FindPartOverride(config, parts[i].name))
				{
					entry.transform.position += partOverride->positionOffset;
					entry.transform.scale *= partOverride->scaleMultiplier;
					entry.transform.rotation = glm::quat(glm::radians(partOverride->rotationOffsetDegrees));
					entry.mesh.visible = partOverride->visible;
					if (!partOverride->textureOverride.empty())
						entry.mesh.texturePath =
						    PortableAssetPath(resolved.parent_path() / partOverride->textureOverride);
				}
				created[0].hierarchy.children.push_back(child);
				created.push_back(std::move(entry));
			}
			context.history.Execute(eastl::make_unique<CreateAssetCommand>(std::move(created)), context.world);
			context.selectedEntity = root;
			context.SetStatus(EditorStatusKind::Success, "Created: " + path.substr(path.find_last_of("/\\") + 1));
			return true;
		}
		catch (const std::exception& e)
		{
			for (const auto& entry : created)
				if (context.world.GetRegistry().valid(entry.entity)) context.world.DestroyEntity(entry.entity);
			context.SetStatus(EditorStatusKind::Danger, e.what());
			NENE_LOG_ERROR("Asset creation failed: {}", e.what());
			return false;
		}
	}
} // namespace NeneEngine

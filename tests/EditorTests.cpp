#include "Core/NeneLogger.h"
#include "Core/ResourceManager.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/DebugDrawSettings.h"
#include "ECS/Events/SceneRestoredEvent.h"
#include "ECS/Systems/RenderSystem.h"
#include "ECS/World.h"
#include "Editor/EditorAssetActions.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorHistory.h"
#include "Editor/EditorSceneSnapshot.h"
#include "Editor/Panels/MaterialEditorPanel.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Graphics/Backend/IRenderAdapter.h"
#include "Graphics/Loaders/ResourceLoaderRegistration.h"
#include "Scene/SceneSerializer.h"
#include <ImGuizmo.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace NeneEngine;

namespace
{
	void Check(bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}

	class RecordingRenderer final : public IRenderAdapter
	{
	  public:
		int meshUploads = 0, textureUploads = 0, shaderUploads = 0;
		uint32_t nextTarget = 1;
		RenderTargetId active;
		std::vector<RenderItem> items;
		bool Init(HWND, uint32_t, uint32_t) override { return true; }
		void Shutdown() override {}
		GPUBuffer CreateVertexBuffer(const void*, uint64_t, uint32_t) override { return {}; }
		GPUBuffer CreateIndexBuffer(const uint32_t*, uint32_t) override { return {}; }
		GPUMesh UploadMesh(const MeshData& data) override
		{
			Check(!data.vertices.empty() && !data.indices.empty(), "Uploaded empty mesh");
			return {MeshId{static_cast<uint32_t>(++meshUploads)}};
		}
		GPUTexture CreateTexture2D(const TextureResource&) override
		{
			return {TextureId{static_cast<uint32_t>(++textureUploads)}, 64, 32};
		}
		GPUShaderProgram CreateShaderProgram(const ShaderProgramResource&) override
		{
			return {ShaderId{static_cast<uint32_t>(++shaderUploads)}};
		}
		RenderTarget CreateRenderTarget(uint32_t w, uint32_t h) override
		{
			return {RenderTargetId{nextTarget++}, w, h};
		}
		void DestroyRenderTarget(RenderTargetId id) override
		{
			if (active == id) active = {};
		}
		void SetRenderTarget(RenderTargetId id) override { active = id; }
		RenderTargetId GetRenderTarget() const override { return active; }
		uintptr_t GetRenderTargetTextureHandle(RenderTargetId id) const override { return id.value; }
		uintptr_t GetTextureHandle(TextureId id) const override { return id.value; }
		void BeginFrame() override { items.clear(); }
		void SubmitRenderItem(const RenderItem& item) override { items.push_back(item); }
		void EndFrame() override {}
		void Present(uint32_t) override {}
		void Resize(uint32_t, uint32_t) override {}
		void SetClearColor(const glm::vec4&) override {}
	};

	ECS::Entity Find(const ECS::World& world, const std::string& name)
	{
		for (const auto [entity, tag] : world.GetRegistry().view<const ECS::TagComponent>().each())
			if (tag.name == name) return entity;
		return ECS::NullEntity;
	}

	void SceneRoundTrip()
	{
		ECS::World world;
		const auto camera = world.CreateEntity("Application Camera");
		world.AddComponent<ECS::CameraComponent>(camera).isPrimary = true;
		world.AddComponent<ECS::TransformComponent>(camera);
		const auto root = world.CreateEntity("Root");
		world.AddComponent<ECS::TransformComponent>(root).position = {1, 2, 3};
		const auto child = world.CreateEntity("Model");
		world.AddComponent<ECS::TransformComponent>(child);
		world.AddComponent<ECS::HierarchyComponent>(root).children.push_back(child);
		world.AddComponent<ECS::HierarchyComponent>(child).parent = root;
		auto& material = world.AddComponent<ECS::MeshRendererComponent>(child);
		material.meshPath = "assets/models/example.obj";
		material.meshPart = 2;
		material.texturePath = "assets/texture.png";
		material.shaderPath = "assets/shaders/textured_mesh.shader";
		material.tint = {0.1f, 0.2f, 0.3f, 0.4f};
		const auto original = SceneSerializer::Serialize(world);
		int restored = 0;
		world.GetEventBus().Subscribe<ECS::SceneRestoredEvent>([&](const auto&) { ++restored; });
		world.GetRegistry().ctx().emplace<ECS::DebugDrawSettings>().drawColliders = true;
		SceneSerializer::Deserialize(original, world, true);
		Check(Find(world, "Application Camera") == camera, "Application camera identity changed on load");
		Check(world.GetRegistry().ctx().get<ECS::DebugDrawSettings>().drawColliders, "Registry settings lost");
		const auto* loaded = world.GetComponent<ECS::MeshRendererComponent>(Find(world, "Model"));
		Check(loaded && loaded->meshPath == "assets/models/example.obj" && loaded->meshPart == 2,
		      "Model reference lost");
		Check(loaded->texturePath == "assets/texture.png" &&
		          loaded->shaderPath == "assets/shaders/textured_mesh.shader",
		      "Material references lost");
		Check(loaded->tint == glm::vec4(0.1f, 0.2f, 0.3f, 0.4f), "Material tint lost");
		Check(world.GetComponent<ECS::HierarchyComponent>(Find(world, "Model"))->parent == Find(world, "Root"),
		      "Hierarchy was not remapped");
		Check(restored == 1, "Scene restored event not emitted");
		const auto beforeFailure = SceneSerializer::Serialize(world);
		for (int failure = 0; failure < 3; ++failure)
		{
			auto broken = original;
			if (failure == 0) broken["entities"][0]["components"]["Transform"]["position"] = nullptr;
			if (failure == 1) broken["entities"].push_back(broken["entities"][0]);
			if (failure == 2) broken["entities"][0]["components"]["HierarchyComponent"] = {{"parent", 999999}};
			bool threw = false;
			try
			{
				SceneSerializer::Deserialize(broken, world, true);
			}
			catch (...)
			{
				threw = true;
			}
			Check(threw, "Invalid scene accepted");
			Check(SceneSerializer::Serialize(world) == beforeFailure, "Failed load modified live scene");
		}
		const auto path = std::filesystem::path("out/qa/roundtrip.scene.json");
		SceneSerializer::SaveToFile(world, path);
		world.GetComponent<ECS::MeshRendererComponent>(Find(world, "Model"))->tint = glm::vec4(1);
		SceneSerializer::SaveToFile(world, path); // Replacing an existing file must work on Windows.
		SceneSerializer::LoadFromFile(path, world, true);
		Check(world.GetComponent<ECS::MeshRendererComponent>(Find(world, "Model"))->tint == glm::vec4(1),
		      "Overwrite did not persist");
		auto legacy = original;
		for (auto& entity : legacy["entities"])
			if (auto it = entity["components"].find("MeshRenderer"); it != entity["components"].end())
			{
				it->erase("meshPath");
				it->erase("meshPart");
				(*it)["material"].erase("texturePath");
				(*it)["material"].erase("shaderPath");
			}
		SceneSerializer::Deserialize(legacy, world);
		Check(world.GetComponent<ECS::MeshRendererComponent>(Find(world, "Model"))->meshPath.empty(),
		      "Legacy scene defaults broken");
	}

	void AssetsAndHistory()
	{
		ECS::World world;
		EditorHistory history;
		EditorContext context{world, history};
		RecordingRenderer renderer;
		context.renderer = &renderer;
		context.assets.Refresh();
		Check(!context.assets.Entries().empty(), "Asset scan returned no files");
		const std::string texture = "assets/models/momosuzu_nene_posed/momosuzu_nene_posed_base_color.png";
		Check(CreateSceneAsset(context, texture, true, {2, 3, 4}), "Texture drop failed");
		const auto created = context.selectedEntity;
		Check(world.GetComponent<ECS::TransformComponent>(created)->position == glm::vec3(2, 3, 4),
		      "Drop position lost");
		const auto material = *world.GetComponent<ECS::MeshRendererComponent>(created);
		Check(history.Undo(world) && !world.GetRegistry().valid(created), "Creation undo failed");
		Check(history.Redo(world) && world.GetRegistry().valid(created), "Creation redo failed");
		Check(world.GetComponent<ECS::MeshRendererComponent>(created)->texturePath == texture, "Redo lost material");
		const auto binding = context.assetResolver.Resolve(material, renderer);
		Check(binding.meshId.IsValid() && binding.textureId.IsValid() && binding.shaderId.IsValid(),
		      "Asset resolution failed");
		const int uploads = renderer.meshUploads + renderer.textureUploads + renderer.shaderUploads;
		context.assetResolver.Resolve(material, renderer);
		Check(renderer.meshUploads + renderer.textureUploads + renderer.shaderUploads == uploads,
		      "Unchanged assets reuploaded");
		for (const auto primitive : {PrimitiveType::Cube, PrimitiveType::Quad, PrimitiveType::Triangle})
		{
			ECS::MeshRendererComponent primitiveMaterial;
			primitiveMaterial.primitiveType = primitive;
			primitiveMaterial.texturePath = texture;
			primitiveMaterial.shaderPath = material.shaderPath;
			const auto primitiveBinding = context.assetResolver.Resolve(primitiveMaterial, renderer);
			Check(primitiveBinding.meshId.IsValid() && primitiveBinding.shaderId.IsValid(),
			      "Primitive material did not resolve to textured geometry");
		}
		std::ofstream("out/qa/model.obj") << "o Triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
		Check(CreateSceneAsset(context, "out/qa/model.obj", false, {0, 0, 0}), "Model drop failed");
		const auto root = context.selectedEntity;
		const auto children = world.GetComponent<ECS::HierarchyComponent>(root)->children;
		Check(!children.empty(), "Model parts missing");
		Check(history.Undo(world) && !world.GetRegistry().valid(root), "Model root undo failed");
		for (const auto child : children) Check(!world.GetRegistry().valid(child), "Undo left model part");
		Check(history.Redo(world), "Model redo failed");
		for (const auto child : children)
			Check(world.GetComponent<ECS::HierarchyComponent>(child)->parent == root, "Redo broke hierarchy");
	}

	void ViewportGeometry()
	{
		ImGui::CreateContext();
		auto& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		io.DisplaySize = {1000, 700};
		io.DeltaTime = 1.0f / 60;
		unsigned char* pixels;
		int w, h;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
		io.Fonts->SetTexID(1);
		RecordingRenderer renderer;
		ECS::World world;
		EditorHistory history;
		EditorContext context{world, history};
		context.renderer = &renderer;
		context.cameraEntity = world.CreateEntity("Camera");
		world.AddComponent<ECS::CameraComponent>(context.cameraEntity);
		world.AddComponent<ECS::TransformComponent>(context.cameraEntity);
		{
			ViewportPanel panel;
			for (const ImVec2 size : {ImVec2(600, 450), ImVec2(800, 300)})
			{
				ImGui::NewFrame();
				ImGuizmo::BeginFrame();
				ImGui::SetNextWindowPos({70, 80});
				ImGui::SetNextWindowSize(size);
				ImGui::Begin("Viewport");
				panel.Draw(context);
				const auto imageSize = ImGui::GetItemRectSize();
				const auto imageMin = ImGui::GetItemRectMin();
				Check(imageMin.x > 70 && imageMin.y > 100, "Viewport image overlaps window chrome or toolbar");
				Check(std::abs(world.GetComponent<ECS::CameraComponent>(context.cameraEntity)->aspectRatio -
				               imageSize.x / imageSize.y) < 0.001f,
				      "Camera aspect differs from image");
				Check(renderer.GetRenderTarget().IsValid(), "Viewport did not select render target");
				ImGui::End();
				ImGui::Render();
			}
		}
		Check(CreateSceneAsset(context, "assets/models/momosuzu_nene_posed/momosuzu_nene_posed_base_color.png", true,
		                       {0, 0, 0}),
		      "Material object creation failed");
		context.assets.Refresh();
		const auto entity = context.selectedEntity;
		const auto original = *world.GetComponent<ECS::MeshRendererComponent>(entity);
		{
			MaterialEditorPanel panel;
			const auto viewportTarget = renderer.CreateRenderTarget(800, 600);
			renderer.SetRenderTarget(viewportTarget.id);
			ImGui::NewFrame();
			ImGui::SetNextWindowSize({400, 650});
			ImGui::Begin("Material Editor");
			panel.Draw(context);
			ImGui::End();
			ImGui::Render();
			Check(renderer.GetRenderTarget() == viewportTarget.id, "Material editor changed scene render target");
			EditorSceneSnapshot snapshot;
			snapshot.Capture(world);
			auto changed = original;
			changed.tint = {0.2f, 0.4f, 0.6f, 1};
			*world.GetComponent<ECS::MeshRendererComponent>(entity) = changed;
			history.RecordComponentChange("Edit Material", entity, original, changed);
			ECS::RenderSystem system(&renderer, context.cameraEntity);
			renderer.BeginFrame();
			system.Render(world);
			Check(renderer.items.size() == 1 && renderer.items[0].tint == changed.tint,
			      "Material change not visible in scene");
			Check(history.Undo(world), "Material undo failed");
			renderer.BeginFrame();
			system.Render(world);
			Check(renderer.items[0].tint == original.tint, "Material undo left stale render state");
			Check(history.Redo(world), "Material redo failed");
			snapshot.Restore(world);
			Check(world.GetComponent<ECS::MeshRendererComponent>(entity)->tint == original.tint,
			      "Play stop did not restore material");
		}
		ImGui::DestroyContext();
	}
} // namespace

int main()
{
	try
	{
		std::filesystem::create_directories("out/qa");
		RegisterGraphicsResourceLoaders(ResourceManager::GetInstance());
		SceneRoundTrip();
		std::cout << "PASS scene round trip, overwrite, camera identity, invalid load, legacy format\n";
		AssetsAndHistory();
		std::cout << "PASS asset scanning, model/texture creation, undo/redo, resource cache\n";
		ViewportGeometry();
		std::cout << "PASS viewport image geometry, resizing, camera aspect, material undo/redo and "
		             "Play/Stop\n";
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "FAIL " << e.what() << '\n';
		return 1;
	}
}

#include "Core/ResourceManager.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Graphics/Backend/IRenderAdapter.h"
#include "Graphics/Runtime/SceneAssetResolver.h"

#include <chrono>
#include <stdexcept>
#include <thread>

namespace
{
	using namespace NeneEngine;

	class TestRenderer final : public IRenderAdapter
	{
	  public:
		bool rejectUpload = false;
		size_t uploadAttempts = 0;

		bool Init(HWND, uint32_t, uint32_t) override { return true; }
		void Shutdown() override {}
		GPUBuffer CreateVertexBuffer(const void*, uint64_t, uint32_t) override { return {}; }
		GPUBuffer CreateIndexBuffer(const uint32_t*, uint32_t) override { return {}; }
		GPUMesh UploadMesh(const MeshData&) override { return GPUMesh{MeshId{++m_nextId}}; }
		eastl::shared_ptr<IMeshUploadData> PrepareMesh(const MeshData&) const override
		{
			return eastl::make_shared<IMeshUploadData>();
		}
		GPUMesh UploadMesh(const IMeshUploadData&) override
		{
			++uploadAttempts;
			return rejectUpload ? GPUMesh{} : GPUMesh{MeshId{++m_nextId}};
		}
		GPUTexture CreateTexture2D(const TextureResource&) override { return {}; }
		eastl::shared_ptr<ITextureUploadData> PrepareTexture2D(const TextureResource&) const override { return {}; }
		GPUTexture CreateTexture2D(const TextureResource&, const ITextureUploadData&) override { return {}; }
		GPUTexture CreatePlaceholderTexture() override { return {}; }
		GPUShaderProgram CreateShaderProgram(const ShaderProgramResource&) override { return {}; }
		eastl::shared_ptr<IShaderProgramUploadData> PrepareShaderProgram(const ShaderProgramResource&) const override
		{
			return {};
		}
		GPUShaderProgram CreateShaderProgram(const ShaderProgramResource&, const IShaderProgramUploadData&) override
		{
			return {};
		}
		RenderTarget CreateRenderTarget(uint32_t, uint32_t) override { return {}; }
		void DestroyRenderTarget(RenderTargetId) override {}
		void SetRenderTarget(RenderTargetId) override {}
		RenderTargetId GetRenderTarget() const override { return {}; }
		uintptr_t GetRenderTargetTextureHandle(RenderTargetId) const override { return 0; }
		uintptr_t GetTextureHandle(TextureId) const override { return 0; }
		void BeginFrame() override {}
		void SubmitRenderItem(const RenderItem&) override {}
		void EndFrame() override {}
		void Present(uint32_t) override {}
		void Resize(uint32_t, uint32_t) override {}
		void SetClearColor(const glm::vec4&) override {}

	  private:
		uint32_t m_nextId = 0;
	};

	void Check(bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}

	void FinishLoads(ResourceLoadQueue& queue)
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (queue.GetPendingCount() != 0)
		{
			Check(std::chrono::steady_clock::now() < deadline, "Mesh loading timed out");
			queue.Pump();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
} // namespace

void TestAsyncMeshGeometry()
{
	using namespace NeneEngine;
	ResourceManager::GetInstance().RegisterLoader<ImportedModel>(LoadMeshPartsFromFile);
	const std::string path = NENE_TEST_FIXTURE_DIR "/translated-mesh-parts.gltf";
	TestRenderer renderer;
	JobSystem jobs;
	jobs.Initialize(1);
	ResourceLoadQueue queue(jobs);
	SceneAssetResolver resolver(&queue);

	const auto geometry = [&](int part) { return resolver.ResolveGeometry(path, part, PrimitiveType::Cube, renderer); };
	const auto placeholder = geometry(0);
	Check(placeholder.state == ResourceLoadState::Loading && placeholder.mesh.IsValid() &&
	          !placeholder.mesh.localBounds,
	      "CPU loading must use a placeholder with unknown bounds");
	FinishLoads(queue);

	const auto pendingFirst = geometry(0);
	const auto pendingSecond = geometry(1);
	Check(pendingFirst.state == ResourceLoadState::Loading && !pendingFirst.mesh.localBounds &&
	          pendingSecond.state == ResourceLoadState::Loading && !pendingSecond.mesh.localBounds &&
	          pendingFirst.mesh.meshId == placeholder.mesh.meshId && pendingSecond.mesh.meshId == placeholder.mesh.meshId,
	      "Neither part may publish bounds before its GPU upload");
	FinishLoads(queue);

	const auto first = geometry(0);
	const auto second = resolver.Resolve(path, 1, {}, {}, PrimitiveType::Cube, renderer);
	Check(first.state == ResourceLoadState::Ready && first.mesh.localBounds && first.mesh.meshId != placeholder.mesh.meshId &&
	          second.meshState == ResourceLoadState::Ready && second.localBounds && second.meshId != first.mesh.meshId &&
	          first.mesh.localBounds->min.x != second.localBounds->min.x,
	      "Completing both uploads must publish each part's own mesh and bounds");

	renderer.rejectUpload = true;
	Check(geometry(-1).state == ResourceLoadState::Loading && geometry(99).state == ResourceLoadState::Loading,
	      "Both mesh requests must start loading");
	FinishLoads(queue);
	const auto failedUpload = geometry(-1);
	const auto invalidPart = geometry(99);
	Check(failedUpload.state == ResourceLoadState::Failed && !failedUpload.mesh.IsValid() && !failedUpload.mesh.localBounds &&
	          invalidPart.state == ResourceLoadState::Failed && !invalidPart.mesh.localBounds,
	      "A failed upload or invalid part must have unknown bounds");
	resolver.RetryFailed();
	renderer.rejectUpload = false;
	const auto retry = geometry(-1);
	Check(retry.state == ResourceLoadState::Loading && !retry.mesh.localBounds,
	      "Retry must return to loading with unknown bounds");
	FinishLoads(queue);
	const auto merged = geometry(-1);
	Check(merged.state == ResourceLoadState::Ready && merged.mesh.localBounds &&
	          merged.mesh.localBounds->min == glm::vec3(11, 3, 4) && merged.mesh.localBounds->max == glm::vec3(28, 9, 9),
	      "A successful retry must publish the merged model bounds");

	const auto missing = [&]()
	{
		return resolver.ResolveGeometry(path + ".missing", -1, PrimitiveType::Cube, renderer);
	};
	Check(missing().state == ResourceLoadState::Loading, "Import must start asynchronously");
	FinishLoads(queue);
	const auto failedImport = missing();
	Check(failedImport.state == ResourceLoadState::Failed && !failedImport.mesh.IsValid() && !failedImport.mesh.localBounds,
	      "An import failure must have unknown bounds");
}

void TestAsyncVisibilityBounds()
{
	using namespace NeneEngine;
	using namespace NeneEngine::ECS;
	ResourceManager::GetInstance().RegisterLoader<ImportedModel>(LoadMeshPartsFromFile);
	const std::string path = NENE_TEST_FIXTURE_DIR "/translated-mesh-parts.gltf";
	TestRenderer firstRenderer;
	TestRenderer secondRenderer;
	IRenderAdapter* renderers[] = {&firstRenderer, &secondRenderer};
	JobSystem jobs;
	jobs.Initialize(1);
	ResourceLoadQueue queue(jobs);
	SceneAssetResolver resolver(&queue);
	World world;
	const auto sync = [&] { world.SyncVisibility(&resolver, renderers); };
	const auto& visibility = world.GetVisibilityRegistry();
	const auto hasUnknownBounds = [&](Entity entity)
	{
		const auto* bounds = visibility.FindBounds(entity);
		return bounds != nullptr && !*bounds;
	};
	const auto hasBounds = [&](Entity entity, const glm::vec3& expectedMin)
	{
		const auto* bounds = visibility.FindBounds(entity);
		return bounds != nullptr && *bounds && (*bounds)->min == expectedMin;
	};
	const auto create = [&]
	{
		const Entity entity = world.CreateEntity();
		world.AddComponent<TransformComponent>(entity);
		auto& mesh = world.AddComponent<MeshRendererComponent>(entity);
		mesh.meshPath = path;
		mesh.meshPart = 0;
		return entity;
	};
	const auto selectPart = [&](Entity entity, int part)
	{
		world.GetRegistry().patch<MeshRendererComponent>(entity, [part](auto& mesh) { mesh.meshPart = part; });
	};
	const Entity first = create();
	const Entity second = create();
	sync();
	Check(hasUnknownBounds(first) && hasUnknownBounds(second), "Pending imports must have unknown bounds");
	FinishLoads(queue);
	sync();

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (firstRenderer.uploadAttempts + secondRenderer.uploadAttempts == 0)
	{
		Check(std::chrono::steady_clock::now() < deadline, "First adapter upload timed out");
		queue.Pump(1);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	sync();
	Check(firstRenderer.uploadAttempts + secondRenderer.uploadAttempts == 1 && hasUnknownBounds(first),
	      "One adapter completing early must not publish bounds while another shows a placeholder");
	CameraComponent camera;
	const glm::vec3 origin{0.0f};
	const VisibilityCamera query(camera.GetProjectionMatrix() * camera.GetViewMatrix(origin), origin,
	                             camera.forward, camera.farPlane);
	eastl::vector<Entity> visible;
	visibility.QueryVisible(query, visible);
	Check(visible.size() == 2, "The shared visibility set must still include both placeholder candidates");

	FinishLoads(queue);
	sync();
	const glm::vec3 firstPartMin{11.0f, 3.0f, 4.0f};
	Check(hasBounds(first, firstPartMin) && hasBounds(second, firstPartMin) && firstRenderer.uploadAttempts == 1 &&
	          secondRenderer.uploadAttempts == 1,
	      "Both adapters must share one mesh request per part and publish the bounds to all matching entities");
	visibility.QueryVisible(query, visible);
	Check(visible.empty(), "Known imported bounds must allow the camera to cull the mesh");

	secondRenderer.rejectUpload = true;
	selectPart(first, 1);
	sync();
	Check(hasUnknownBounds(first) && hasBounds(second, firstPartMin),
	      "A geometry change must discard only the switched entity's old bounds");
	FinishLoads(queue);
	sync();
	Check(hasUnknownBounds(first), "Failed geometry must remain a candidate with unknown bounds");

	secondRenderer.rejectUpload = false;
	resolver.RetryFailed();
	sync();
	Check(hasUnknownBounds(first), "Retry must keep bounds unknown while the replacement uploads");
	FinishLoads(queue);
	sync();
	Check(hasBounds(first, {26.0f, 6.0f, 9.0f}) && hasBounds(second, firstPartMin) &&
	          firstRenderer.uploadAttempts == 2 && secondRenderer.uploadAttempts == 3,
	      "Retry must recover the failed part without reuploading successful meshes");

	selectPart(first, -1);
	selectPart(second, -1);
	sync();
	world.DestroyEntity(first);
	sync();
	Check(visibility.FindBounds(first) == nullptr && hasUnknownBounds(second),
	      "An entity deleted while its mesh is loading must leave no visibility entry");
	World prepared;
	const Entity replacement = prepared.GetRegistry().create(second);
	Check(replacement == second, "The new scene must reuse the old scene's handle for this regression");
	prepared.AddComponent<TransformComponent>(replacement).position.z = -5.0f;
	prepared.AddComponent<MeshRendererComponent>(replacement);
	world.SwapSceneRegistry(prepared);
	FinishLoads(queue);
	sync();
	Check(visibility.Size() == 1 && hasBounds(replacement, {-0.5f, -0.5f, -5.0f}),
	      "An old scene's completed load must not overwrite the current scene's bounds");
}

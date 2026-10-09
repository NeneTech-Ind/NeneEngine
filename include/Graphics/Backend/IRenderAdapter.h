// IRenderAdapter.h

#pragma once

#include "Graphics/Runtime/RenderTypes.h"

#include <Windows.h>
#include <cstdint>

#include <glm/glm.hpp>

namespace NeneEngine
{

	class IRenderAdapter
	{
	  public:
		virtual ~IRenderAdapter() = default;

		virtual bool Init(HWND hwnd, uint32_t width, uint32_t height) = 0;
		virtual void Shutdown() = 0;

		// Returned ids are backend-local GPU handles, not serialized asset identifiers.
		virtual GPUBuffer CreateVertexBuffer(const void* vertexData, uint64_t sizeBytes, uint32_t vertexCount) = 0;
		virtual GPUBuffer CreateIndexBuffer(const uint32_t* indices, uint32_t indexCount) = 0;
		virtual GPUMesh UploadMesh(const MeshData& meshData) = 0;
		virtual GPUTexture CreateTexture2D(const TextureResource& texture) = 0;
		virtual GPUShaderProgram CreateShaderProgram(const ShaderProgramResource& shaderProgram) = 0;

		[[nodiscard]] virtual uintptr_t GetResourceDomainKey() const { return reinterpret_cast<uintptr_t>(this); }

		virtual void BeginFrame() = 0;
		virtual void SubmitRenderItem(const RenderItem& item) = 0;

		// Debug helpers built on top of SubmitRenderItem with the DebugLine primitive; backends may override them.
		virtual void DrawDebugLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color,
		                           const glm::mat4& viewProjection);
		virtual void DrawDebugAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color,
		                           const glm::mat4& viewProjection);
		virtual void EndFrame() = 0;
		virtual void Present(uint32_t syncInterval) = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual void SetClearColor(const glm::vec4& color) = 0;
	};

} // namespace NeneEngine

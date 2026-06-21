// IRenderAdapter.h

#pragma once

#include "Rendering/RenderTypes.h"

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

		virtual void BeginFrame() = 0;
		virtual void SubmitRenderItem(const RenderItem& item) = 0;
		virtual void EndFrame() = 0;
		virtual void Present() = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual void SetClearColor(const glm::vec4& color) = 0;
	};

} // namespace NeneEngine

// DiligentDX12Adapter.h

#pragma once

#include "IRenderAdapter.h"

#include "../external/DiligentEngine/DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h"
#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h"
#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h"
#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/Texture.h"
#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/TextureView.h"

#include <array>
#include <unordered_map>
#include <vector>

namespace NeneEngine
{

	class DiligentDX12Adapter final : public IRenderAdapter
	{
	  public:
		DiligentDX12Adapter();
		~DiligentDX12Adapter() override;

		bool Init(HWND hwnd, uint32_t width, uint32_t height) override;
		void Shutdown() override;

		GPUBuffer CreateVertexBuffer(const void* vertexData, uint64_t sizeBytes, uint32_t vertexCount) override;
		GPUBuffer CreateIndexBuffer(const uint32_t* indices, uint32_t indexCount) override;
		GPUMesh UploadMesh(const MeshData& meshData) override;
		GPUTexture CreateTexture2D(const TextureResource& texture) override;
		GPUShaderProgram CreateShaderProgram(const ShaderProgramResource& shaderProgram) override;
		void BeginFrame() override;
		void SubmitRenderItem(const RenderItem& item) override;
		void CreateResources();
		void EndFrame() override;
		void Present() override;
		void Resize(uint32_t width, uint32_t height) override;
		void SetClearColor(const glm::vec4& color) override;

	  private:
		static constexpr size_t PrimitiveTypeCount = 4;

		struct PrimitiveDrawConstants
		{
			glm::mat4 modelViewProjectionMatrix = glm::mat4(1.0f);
			glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
		};

		struct UploadedMeshBuffers
		{
			GPUBufferId vertexBufferId{};
			GPUBufferId indexBufferId{};
			Diligent::RefCntAutoPtr<Diligent::IBuffer> vertexBuffer;
			Diligent::RefCntAutoPtr<Diligent::IBuffer> indexBuffer;
			uint32_t vertexCount = 0;
			uint32_t indexCount = 0;
		};

		struct UploadedBuffer
		{
			Diligent::RefCntAutoPtr<Diligent::IBuffer> buffer;
			uint64_t sizeBytes = 0;
			uint32_t elementCount = 0;
		};

		struct UploadedTexture
		{
			Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
			Diligent::RefCntAutoPtr<Diligent::ITextureView> shaderResourceView;
			TextureFilterMode filterMode = TextureFilterMode::Linear;
			TextureAddressMode addressMode = TextureAddressMode::Wrap;
			uint32_t width = 0;
			uint32_t height = 0;
		};

		struct UploadedShaderProgram
		{
			Diligent::RefCntAutoPtr<Diligent::IPipelineState> linearWrapPipelineState;
			Diligent::RefCntAutoPtr<Diligent::IPipelineState> nearestWrapPipelineState;
			Diligent::RefCntAutoPtr<Diligent::IPipelineState> linearClampPipelineState;
			Diligent::RefCntAutoPtr<Diligent::IPipelineState> nearestClampPipelineState;
			std::unordered_map<uint32_t, Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>> srbsByTexture;
		};

		Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_pDevice;
		Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pImmediateContext;
		Diligent::RefCntAutoPtr<Diligent::ISwapChain> m_pSwapChain;

		std::array<Diligent::RefCntAutoPtr<Diligent::IPipelineState>, PrimitiveTypeCount> m_pPrimitivePSOs;
		std::array<Diligent::RefCntAutoPtr<Diligent::IBuffer>, PrimitiveTypeCount> m_pPrimitiveConstantBuffers;
		std::array<Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>, PrimitiveTypeCount> m_pPrimitiveSRBs;
		Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pMeshPSO;
		Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pMeshConstantBuffer;
		Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pMeshSRB;
		std::vector<RenderItem> m_renderQueue;
		std::unordered_map<uint32_t, UploadedBuffer> m_uploadedBuffers;
		std::unordered_map<uint32_t, UploadedMeshBuffers> m_uploadedMeshes;
		std::unordered_map<uint32_t, UploadedTexture> m_uploadedTextures;
		std::unordered_map<uint32_t, UploadedShaderProgram> m_uploadedShaderPrograms;
		glm::vec4 m_clearColor{0.1f, 0.1f, 0.2f, 1.0f};
		uint32_t m_nextBufferId = 1;
		uint32_t m_nextMeshId = 1;
		uint32_t m_nextTextureId = 1;
		uint32_t m_nextShaderId = 1;

		[[nodiscard]] Diligent::IPipelineState* GetPipelineState(PrimitiveType primitiveType) const;
		[[nodiscard]] const UploadedMeshBuffers* GetUploadedMesh(MeshId meshId) const;
		[[nodiscard]] const UploadedTexture* GetUploadedTexture(TextureId textureId) const;
		[[nodiscard]] UploadedShaderProgram* GetUploadedShaderProgram(ShaderId shaderId);
		[[nodiscard]] Diligent::IPipelineState* GetShaderPipelineState(UploadedShaderProgram& shaderProgram,
		                                                               TextureFilterMode filterMode,
		                                                               TextureAddressMode addressMode) const;
		[[nodiscard]] uint32_t GetVertexCount(PrimitiveType primitiveType) const;
		[[nodiscard]] Diligent::IShaderResourceBinding* GetShaderResourceBinding(UploadedShaderProgram& shaderProgram,
		                                                                         TextureId textureId);
	};

} // namespace NeneEngine

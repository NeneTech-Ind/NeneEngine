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

#include <EASTL/shared_ptr.h>
#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>
#include <array>

namespace NeneEngine
{

	struct DiligentDX12SharedDevice
	{
		static constexpr size_t PrimitiveTypeCount = 5;

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
			eastl::unordered_map<uint32_t, Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>> srbsByTexture;
		};

		~DiligentDX12SharedDevice();

		Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
		Diligent::RefCntAutoPtr<Diligent::IDeviceContext> immediateContext;

		Diligent::TEXTURE_FORMAT colorBufferFormat = Diligent::TEX_FORMAT_UNKNOWN;
		Diligent::TEXTURE_FORMAT depthBufferFormat = Diligent::TEX_FORMAT_UNKNOWN;
		bool resourcesCreated = false;

		std::array<Diligent::RefCntAutoPtr<Diligent::IPipelineState>, PrimitiveTypeCount> primitivePSOs;
		std::array<Diligent::RefCntAutoPtr<Diligent::IBuffer>, PrimitiveTypeCount> primitiveConstantBuffers;
		std::array<Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>, PrimitiveTypeCount> primitiveSRBs;
		Diligent::RefCntAutoPtr<Diligent::IPipelineState> meshPSO;
		Diligent::RefCntAutoPtr<Diligent::IBuffer> meshConstantBuffer;
		Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> meshSRB;
		eastl::unordered_map<uint32_t, UploadedBuffer> uploadedBuffers;
		eastl::unordered_map<uint32_t, UploadedMeshBuffers> uploadedMeshes;
		eastl::unordered_map<uint32_t, UploadedTexture> uploadedTextures;
		eastl::unordered_map<uint32_t, UploadedShaderProgram> uploadedShaderPrograms;
		uint32_t nextBufferId = 1;
		uint32_t nextMeshId = 1;
		uint32_t nextTextureId = 1;
		uint32_t nextShaderId = 1;
	};

	class DiligentDX12Adapter final : public IRenderAdapter
	{
	  public:
		explicit DiligentDX12Adapter(eastl::shared_ptr<DiligentDX12SharedDevice> sharedDevice);
		~DiligentDX12Adapter() override;

		bool Init(HWND hwnd, uint32_t width, uint32_t height) override;
		void Shutdown() override;

		GPUBuffer CreateVertexBuffer(const void* vertexData, uint64_t sizeBytes, uint32_t vertexCount) override;
		GPUBuffer CreateIndexBuffer(const uint32_t* indices, uint32_t indexCount) override;
		GPUMesh UploadMesh(const MeshData& meshData) override;
		GPUTexture CreateTexture2D(const TextureResource& texture) override;
		GPUShaderProgram CreateShaderProgram(const ShaderProgramResource& shaderProgram) override;
		[[nodiscard]] uintptr_t GetResourceDomainKey() const override;
		void BeginFrame() override;
		void SubmitRenderItem(const RenderItem& item) override;
		bool CreateResources();
		void EndFrame() override;
		void Present(uint32_t syncInterval) override;
		void Resize(uint32_t width, uint32_t height) override;
		void SetClearColor(const glm::vec4& color) override;

	  private:
		using UploadedMeshBuffers = DiligentDX12SharedDevice::UploadedMeshBuffers;
		using UploadedBuffer = DiligentDX12SharedDevice::UploadedBuffer;
		using UploadedTexture = DiligentDX12SharedDevice::UploadedTexture;
		using UploadedShaderProgram = DiligentDX12SharedDevice::UploadedShaderProgram;

		struct PrimitiveDrawConstants
		{
			glm::mat4 modelViewProjectionMatrix = glm::mat4(1.0f);
			glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
		};

		eastl::shared_ptr<DiligentDX12SharedDevice> m_shared;
		Diligent::RefCntAutoPtr<Diligent::ISwapChain> m_pSwapChain;
		eastl::vector<RenderItem> m_renderQueue;
		glm::vec4 m_clearColor{0.1f, 0.1f, 0.2f, 1.0f};

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

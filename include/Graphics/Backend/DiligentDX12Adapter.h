// DiligentDX12Adapter.h

#pragma once

#include "Graphics/Backend/IRenderAdapter.h"

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
#include <EASTL/unordered_set.h>
#include <EASTL/vector.h>
#include <array>

namespace Diligent
{
	struct IEngineFactoryD3D12;
}

namespace NeneEngine
{

	struct DiligentDX12SharedDevice
	{
		static constexpr size_t PrimitiveTypeCount = static_cast<size_t>(PrimitiveType::DebugLine) + 1;
		static constexpr size_t SamplerVariantCount = 4;

		struct DrawPipeline
		{
			Diligent::RefCntAutoPtr<Diligent::IPipelineState> pipelineState;
			Diligent::RefCntAutoPtr<Diligent::IBuffer> constantBuffer;
			Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> shaderResourceBinding;
		};

		struct UploadedMesh
		{
			Diligent::RefCntAutoPtr<Diligent::IBuffer> vertexBuffer;
			Diligent::RefCntAutoPtr<Diligent::IBuffer> indexBuffer;
			uint32_t indexCount = 0;
		};

		struct UploadedTexture
		{
			Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
			Diligent::RefCntAutoPtr<Diligent::ITextureView> shaderResourceView;
			TextureFilterMode filterMode = TextureFilterMode::Linear;
			TextureAddressMode addressMode = TextureAddressMode::Wrap;
		};

		struct UploadedShaderProgram
		{
			// Immutable samplers require separate PSOs for nearest and linear texture filtering.
			std::array<Diligent::RefCntAutoPtr<Diligent::IPipelineState>, SamplerVariantCount> pipelineStates;
			eastl::unordered_map<uint32_t, Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>> srbsByTexture;
		};

		~DiligentDX12SharedDevice();

		bool InitializeDevice(Diligent::IEngineFactoryD3D12& factory);
		bool InitializePipelines(Diligent::TEXTURE_FORMAT colorFormat, Diligent::TEXTURE_FORMAT depthFormat);

		Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
		Diligent::RefCntAutoPtr<Diligent::IDeviceContext> immediateContext;

		Diligent::TEXTURE_FORMAT colorBufferFormat = Diligent::TEX_FORMAT_UNKNOWN;
		Diligent::TEXTURE_FORMAT depthBufferFormat = Diligent::TEX_FORMAT_UNKNOWN;
		bool pipelinesCreated = false;

		std::array<DrawPipeline, PrimitiveTypeCount> primitivePipelines;
		DrawPipeline meshPipeline;

		eastl::unordered_map<uint32_t, Diligent::RefCntAutoPtr<Diligent::IBuffer>> uploadedBuffers;
		eastl::unordered_map<uint32_t, UploadedMesh> uploadedMeshes;
		eastl::unordered_map<uint32_t, UploadedTexture> uploadedTextures;
		eastl::unordered_map<uint32_t, UploadedShaderProgram> uploadedShaderPrograms;
		uint32_t nextBufferId = 1;
		uint32_t nextMeshId = 1;
		uint32_t nextTextureId = 1;
		uint32_t nextShaderId = 1;

	  private:
		bool CreatePrimitivePipeline(PrimitiveType primitiveType, const char* name, const char* vertexShaderBody);
		bool CreateMeshPipeline();
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
		void EndFrame() override;
		void Present(uint32_t syncInterval) override;
		void Resize(uint32_t width, uint32_t height) override;
		void SetClearColor(const glm::vec4& color) override;

		[[nodiscard]] Diligent::IRenderDevice* GetDevice() const { return m_shared->device; }
		[[nodiscard]] Diligent::IDeviceContext* GetImmediateContext() const { return m_shared->immediateContext; }
		[[nodiscard]] Diligent::ISwapChain* GetSwapChain() const { return m_swapChain; }

	  private:
		using DrawPipeline = DiligentDX12SharedDevice::DrawPipeline;
		using UploadedMesh = DiligentDX12SharedDevice::UploadedMesh;
		using UploadedShaderProgram = DiligentDX12SharedDevice::UploadedShaderProgram;

		[[nodiscard]] bool IsDeviceReady(const char* operation) const;
		[[nodiscard]] GPUBufferId CreateImmutableBuffer(const char* name, Diligent::BIND_FLAGS bindFlags,
		                                                const void* data, uint64_t sizeBytes);
		void DrawMesh(const RenderItem& item, const UploadedMesh& mesh);
		void DrawPrimitive(const RenderItem& item);
		[[nodiscard]] bool WriteDrawConstants(Diligent::IBuffer* constantBuffer, const RenderItem& item);
		[[nodiscard]] Diligent::IShaderResourceBinding* GetShaderResourceBinding(UploadedShaderProgram& shaderProgram,
		                                                                         TextureId textureId);

		eastl::shared_ptr<DiligentDX12SharedDevice> m_shared;
		Diligent::RefCntAutoPtr<Diligent::ISwapChain> m_swapChain;
		eastl::vector<RenderItem> m_renderQueue;
		eastl::unordered_set<uint64_t> m_reportedTextureFallbacks;
		glm::vec4 m_clearColor{0.1f, 0.1f, 0.2f, 1.0f};
	};

} // namespace NeneEngine

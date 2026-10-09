// DiligentDX12Adapter.cpp

#include "Graphics/Backend/DiligentDX12Adapter.h"

#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/Shader.h"
#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "../external/DiligentEngine/DiligentTools/TextureLoader/interface/TextureLoader.h"
#include "Graphics/Loaders/TextureLoader.h"

#include "Core/NeneLogger.h"

namespace NeneEngine
{

	using namespace Diligent;

	DiligentDX12SharedDevice::~DiligentDX12SharedDevice()
	{
		if (immediateContext) immediateContext->Flush();
		if (device) device->IdleGPU();
	}

	DiligentDX12Adapter::DiligentDX12Adapter(eastl::shared_ptr<DiligentDX12SharedDevice> sharedDevice)
	    : m_shared(std::move(sharedDevice))
	{
	}

	DiligentDX12Adapter::~DiligentDX12Adapter()
	{
		Shutdown();
	}

	bool DiligentDX12Adapter::Init(HWND hwnd, uint32_t width, uint32_t height)
	{
		if (!m_shared)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: shared device state is missing");
			return false;
		}

		IEngineFactoryD3D12* pFactory = LoadAndGetEngineFactoryD3D12();
		if (!pFactory)
		{
			NENE_LOG_ERROR("Failed to load D3D12 Engine Factory");
			return false;
		}

		if (!m_shared->device || !m_shared->immediateContext)
		{
			EngineD3D12CreateInfo engineCreateInfo{};
			engineCreateInfo.EnableValidation = true;

			pFactory->CreateDeviceAndContextsD3D12(engineCreateInfo, &m_shared->device, &m_shared->immediateContext);

			if (!m_shared->device || !m_shared->immediateContext)
			{
				NENE_LOG_ERROR("Failed to create D3D12 device and immediate context");
				m_shared->immediateContext.Release();
				m_shared->device.Release();
				return false;
			}

			NENE_LOG_INFO("DiligentDX12Adapter: created shared D3D12 device");
		}

		SwapChainDesc swapChainCreateDesc{};
		swapChainCreateDesc.Width = width;
		swapChainCreateDesc.Height = height;
		swapChainCreateDesc.BufferCount = 2; // Double buffering
		swapChainCreateDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;

		Win32NativeWindow nativeWindow{hwnd};
		FullScreenModeDesc fullScreenDesc{};

		pFactory->CreateSwapChainD3D12(m_shared->device, m_shared->immediateContext, swapChainCreateDesc,
		                               fullScreenDesc, nativeWindow, &m_pSwapChain);

		if (!m_pSwapChain)
		{
			NENE_LOG_ERROR("Failed to create D3D12 swap chain");
			return false;
		}

		const auto& swapChainDesc = m_pSwapChain->GetDesc();
		NENE_LOG_INFO("DiligentDX12Adapter: swap chain formats color={} depth={}",
		              static_cast<int>(swapChainDesc.ColorBufferFormat),
		              static_cast<int>(swapChainDesc.DepthBufferFormat));

		if (!m_shared->resourcesCreated)
		{
			if (!CreateResources())
			{
				NENE_LOG_ERROR("Failed to create D3D12 render resources");
				Shutdown();
				return false;
			}

			m_shared->colorBufferFormat = swapChainDesc.ColorBufferFormat;
			m_shared->depthBufferFormat = swapChainDesc.DepthBufferFormat;
			m_shared->resourcesCreated = true;
		}
		else if (swapChainDesc.ColorBufferFormat != m_shared->colorBufferFormat ||
		         swapChainDesc.DepthBufferFormat != m_shared->depthBufferFormat)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: swap chain formats do not match the shared pipelines");
			Shutdown();
			return false;
		}

		NENE_LOG_INFO("Diligent DX12 Adapter initialized successfully ({}x{})", width, height);

		return true;
	}

	void DiligentDX12Adapter::Shutdown()
	{
		if (m_shared && m_shared->immediateContext) m_shared->immediateContext->Flush();

		m_renderQueue.clear();
		m_pSwapChain.Release();
	}

	uintptr_t DiligentDX12Adapter::GetResourceDomainKey() const
	{
		return reinterpret_cast<uintptr_t>(m_shared.get());
	}

	GPUBuffer DiligentDX12Adapter::CreateVertexBuffer(const void* vertexData, uint64_t sizeBytes, uint32_t vertexCount)
	{
		if (!m_shared->device)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: CreateVertexBuffer called before device initialization");
			return {};
		}

		if (vertexData == nullptr || sizeBytes == 0 || vertexCount == 0)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: CreateVertexBuffer received empty data");
			return {};
		}

		BufferDesc vertexBufferDesc{};
		vertexBufferDesc.Name = "Mesh Vertex Buffer";
		vertexBufferDesc.BindFlags = BIND_VERTEX_BUFFER;
		vertexBufferDesc.Usage = USAGE_IMMUTABLE;
		vertexBufferDesc.Size = static_cast<Uint64>(sizeBytes);

		BufferData vertexBufferData{};
		vertexBufferData.pData = vertexData;
		vertexBufferData.DataSize = vertexBufferDesc.Size;

		UploadedBuffer uploadedBuffer{};
		m_shared->device->CreateBuffer(vertexBufferDesc, &vertexBufferData, &uploadedBuffer.buffer);
		if (!uploadedBuffer.buffer)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: failed to create vertex buffer (bytes={}, vertices={})", sizeBytes,
			               vertexCount);
			return {};
		}

		uploadedBuffer.sizeBytes = sizeBytes;
		uploadedBuffer.elementCount = vertexCount;

		const GPUBufferId bufferId{m_shared->nextBufferId++};
		m_shared->uploadedBuffers.emplace(bufferId.value, uploadedBuffer);
		return GPUBuffer{bufferId, sizeBytes, vertexCount};
	}

	GPUBuffer DiligentDX12Adapter::CreateIndexBuffer(const uint32_t* indices, uint32_t indexCount)
	{
		if (!m_shared->device)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: CreateIndexBuffer called before device initialization");
			return {};
		}

		if (indices == nullptr || indexCount == 0)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: CreateIndexBuffer received empty data");
			return {};
		}

		BufferDesc indexBufferDesc{};
		indexBufferDesc.Name = "Mesh Index Buffer";
		indexBufferDesc.BindFlags = BIND_INDEX_BUFFER;
		indexBufferDesc.Usage = USAGE_IMMUTABLE;
		indexBufferDesc.Size = static_cast<Uint64>(indexCount * sizeof(uint32_t));

		BufferData indexBufferData{};
		indexBufferData.pData = indices;
		indexBufferData.DataSize = indexBufferDesc.Size;

		UploadedBuffer uploadedBuffer{};
		m_shared->device->CreateBuffer(indexBufferDesc, &indexBufferData, &uploadedBuffer.buffer);
		if (!uploadedBuffer.buffer)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: failed to create index buffer (indices={})", indexCount);
			return {};
		}

		uploadedBuffer.sizeBytes = indexBufferDesc.Size;
		uploadedBuffer.elementCount = indexCount;

		const GPUBufferId bufferId{m_shared->nextBufferId++};
		m_shared->uploadedBuffers.emplace(bufferId.value, uploadedBuffer);
		return GPUBuffer{bufferId, indexBufferDesc.Size, indexCount};
	}

	GPUMesh DiligentDX12Adapter::UploadMesh(const MeshData& meshData)
	{
		if (meshData.vertices.empty() || meshData.indices.empty())
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: UploadMesh received empty mesh data");
			return {};
		}

		const GPUBuffer vertexBuffer =
		    CreateVertexBuffer(meshData.vertices.data(), meshData.vertices.size() * sizeof(Vertex),
		                       static_cast<uint32_t>(meshData.vertices.size()));
		const GPUBuffer indexBuffer =
		    CreateIndexBuffer(meshData.indices.data(), static_cast<uint32_t>(meshData.indices.size()));
		if (!vertexBuffer.IsValid() || !indexBuffer.IsValid()) return {};

		const auto vertexIt = m_shared->uploadedBuffers.find(vertexBuffer.bufferId.value);
		const auto indexIt = m_shared->uploadedBuffers.find(indexBuffer.bufferId.value);
		if (vertexIt == m_shared->uploadedBuffers.end() || indexIt == m_shared->uploadedBuffers.end()) return {};

		UploadedMeshBuffers uploadedMesh{};
		uploadedMesh.vertexBufferId = vertexBuffer.bufferId;
		uploadedMesh.indexBufferId = indexBuffer.bufferId;
		uploadedMesh.vertexBuffer = vertexIt->second.buffer;
		uploadedMesh.indexBuffer = indexIt->second.buffer;
		uploadedMesh.vertexCount = static_cast<uint32_t>(meshData.vertices.size());
		uploadedMesh.indexCount = static_cast<uint32_t>(meshData.indices.size());

		const MeshId meshId{m_shared->nextMeshId++};
		m_shared->uploadedMeshes.emplace(meshId.value, uploadedMesh);

		NENE_LOG_INFO("DiligentDX12Adapter: uploaded mesh {} (vertices={}, indices={})", meshId.value,
		              uploadedMesh.vertexCount, uploadedMesh.indexCount);

		return GPUMesh{meshId, vertexBuffer.bufferId, indexBuffer.bufferId, uploadedMesh.vertexCount,
		               uploadedMesh.indexCount};
	}

	GPUTexture DiligentDX12Adapter::CreateTexture2D(const TextureResource& texture)
	{
		if (!m_shared->device)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: CreateTexture2D called before device initialization");
			return {};
		}

		TextureLoadInfo loadInfo{};
		loadInfo.IsSRGB = texture.isSrgb;

		RefCntAutoPtr<ITextureLoader> textureLoader;
		CreateTextureLoaderFromFile(texture.path.c_str(), IMAGE_FILE_FORMAT_UNKNOWN, loadInfo, &textureLoader);
		if (!textureLoader)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: failed to create texture loader for '{}'", texture.path);
			return {};
		}

		UploadedTexture uploadedTexture{};
		textureLoader->CreateTexture(m_shared->device, &uploadedTexture.texture);
		if (!uploadedTexture.texture)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: failed to create GPU texture for '{}'", texture.path);
			return {};
		}

		const auto textureDesc = uploadedTexture.texture->GetDesc();
		uploadedTexture.filterMode = texture.filterMode;
		uploadedTexture.addressMode = texture.addressMode;
		uploadedTexture.width = textureDesc.Width;
		uploadedTexture.height = textureDesc.Height;
		uploadedTexture.shaderResourceView = uploadedTexture.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
		if (!uploadedTexture.shaderResourceView)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: texture '{}' has no shader resource view", texture.path);
			return {};
		}

		if (m_shared->immediateContext)
		{
			StateTransitionDesc barrier(uploadedTexture.texture, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE,
			                            STATE_TRANSITION_FLAG_UPDATE_STATE);
			m_shared->immediateContext->TransitionResourceState(barrier);
		}

		const TextureId textureId{m_shared->nextTextureId++};
		m_shared->uploadedTextures.emplace(textureId.value, uploadedTexture);
		NENE_LOG_INFO("DiligentDX12Adapter: uploaded texture {} '{}' ({}x{})", textureId.value, texture.path,
		              textureDesc.Width, textureDesc.Height);
		return GPUTexture{textureId, textureDesc.Width, textureDesc.Height};
	}

	GPUShaderProgram DiligentDX12Adapter::CreateShaderProgram(const ShaderProgramResource& shaderProgram)
	{
		if (!m_shared->device || !m_pSwapChain)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: CreateShaderProgram called before renderer initialization");
			return {};
		}

		GraphicsPipelineStateCreateInfo psoCreateInfo{};
		psoCreateInfo.PSODesc.Name = "Resource Shader Program PSO";
		psoCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
		psoCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
		psoCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
		psoCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
		psoCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		psoCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
		psoCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
		psoCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
		psoCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;

		LayoutElement meshLayoutElements[] = {LayoutElement{0, 0, 3, VT_FLOAT32, false},
		                                      LayoutElement{1, 0, 3, VT_FLOAT32, false},
		                                      LayoutElement{2, 0, 2, VT_FLOAT32, false}};
		psoCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = meshLayoutElements;
		psoCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(meshLayoutElements);

		ShaderResourceVariableDesc variables[] = {
		    {SHADER_TYPE_VERTEX, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
		    {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
		psoCreateInfo.PSODesc.ResourceLayout.Variables = variables;
		psoCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(variables);

		ShaderCreateInfo shaderCI{};
		shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
		shaderCI.Desc.UseCombinedTextureSamplers = true;

		RefCntAutoPtr<IShader> vertexShader;
		RefCntAutoPtr<IShader> pixelShader;

		shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
		shaderCI.Desc.Name = shaderProgram.vertexPath.c_str();
		shaderCI.Source = shaderProgram.vertexSource.c_str();
		m_shared->device->CreateShader(shaderCI, &vertexShader);

		shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
		shaderCI.Desc.Name = shaderProgram.pixelPath.c_str();
		shaderCI.Source = shaderProgram.pixelSource.c_str();
		m_shared->device->CreateShader(shaderCI, &pixelShader);

		if (!vertexShader || !pixelShader)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: failed to create shader program '{}', '{}'", shaderProgram.vertexPath,
			               shaderProgram.pixelPath);
			return {};
		}

		psoCreateInfo.pVS = vertexShader;
		psoCreateInfo.pPS = pixelShader;

		UploadedShaderProgram uploadedShader{};

		const auto createPipelineStateWithSampler =
		    [this, &psoCreateInfo, &uploadedShader](TextureFilterMode filterMode, TextureAddressMode addressMode,
		                                            RefCntAutoPtr<IPipelineState>& pipelineState)
		{
			// Immutable samplers require separate PSOs for nearest and linear texture filtering.
			GraphicsPipelineStateCreateInfo samplerPsoCreateInfo = psoCreateInfo;
			SamplerDesc samplerDesc{};
			const FILTER_TYPE filterType =
			    filterMode == TextureFilterMode::Nearest ? FILTER_TYPE_POINT : FILTER_TYPE_LINEAR;
			const TEXTURE_ADDRESS_MODE diligentAddressMode =
			    addressMode == TextureAddressMode::Clamp ? TEXTURE_ADDRESS_CLAMP : TEXTURE_ADDRESS_WRAP;
			samplerDesc.MinFilter = filterType;
			samplerDesc.MagFilter = filterType;
			samplerDesc.MipFilter = filterType;
			samplerDesc.AddressU = diligentAddressMode;
			samplerDesc.AddressV = diligentAddressMode;
			samplerDesc.AddressW = diligentAddressMode;

			ImmutableSamplerDesc immutableSamplers[] = {{SHADER_TYPE_PIXEL, "g_Texture", samplerDesc}};
			samplerPsoCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = immutableSamplers;
			samplerPsoCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(immutableSamplers);

			m_shared->device->CreateGraphicsPipelineState(samplerPsoCreateInfo, &pipelineState);
			if (!pipelineState) return false;

			if (m_shared->meshConstantBuffer)
			{
				auto* constantsVariable = pipelineState->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
				if (constantsVariable != nullptr) constantsVariable->Set(m_shared->meshConstantBuffer);
			}

			return true;
		};

		if (!createPipelineStateWithSampler(TextureFilterMode::Linear, TextureAddressMode::Wrap,
		                                    uploadedShader.linearWrapPipelineState) ||
		    !createPipelineStateWithSampler(TextureFilterMode::Nearest, TextureAddressMode::Wrap,
		                                    uploadedShader.nearestWrapPipelineState) ||
		    !createPipelineStateWithSampler(TextureFilterMode::Linear, TextureAddressMode::Clamp,
		                                    uploadedShader.linearClampPipelineState) ||
		    !createPipelineStateWithSampler(TextureFilterMode::Nearest, TextureAddressMode::Clamp,
		                                    uploadedShader.nearestClampPipelineState))
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: failed to create shader program pipelines");
			return {};
		}

		const ShaderId shaderId{m_shared->nextShaderId++};
		m_shared->uploadedShaderPrograms.emplace(shaderId.value, std::move(uploadedShader));
		NENE_LOG_INFO("DiligentDX12Adapter: created shader program {}", shaderId.value);
		return GPUShaderProgram{shaderId};
	}

	void DiligentDX12Adapter::BeginFrame()
	{
		if (!m_pSwapChain || !m_shared->immediateContext) return;

		ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
		ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();
		if (pRTV == nullptr)
		{
			NENE_LOG_WARN("DiligentDX12Adapter: current back buffer RTV is missing");
			return;
		}

		m_shared->immediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		const float clearColor[] = {m_clearColor.r, m_clearColor.g, m_clearColor.b, m_clearColor.a};
		m_shared->immediateContext->ClearRenderTarget(pRTV, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		if (pDSV != nullptr)
			m_shared->immediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.0f, 0,
			                                              RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		else
			NENE_LOG_WARN("DiligentDX12Adapter: depth buffer DSV is missing");

		m_renderQueue.clear();
	}

	void DiligentDX12Adapter::SubmitRenderItem(const RenderItem& item)
	{
		// Queueing keeps ECS submission separate from the backend draw execution in EndFrame().
		m_renderQueue.push_back(item);
	}

	void DiligentDX12Adapter::EndFrame()
	{
		if (!m_shared->immediateContext || !m_pSwapChain) return;

		const auto& swapChainDesc = m_pSwapChain->GetDesc();
		Viewport viewport{};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(swapChainDesc.Width);
		viewport.Height = static_cast<float>(swapChainDesc.Height);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		m_shared->immediateContext->SetViewports(1, &viewport, swapChainDesc.Width, swapChainDesc.Height);

		for (const auto& item : m_renderQueue)
		{
			if (const UploadedMeshBuffers* uploadedMesh = GetUploadedMesh(item.meshId); uploadedMesh != nullptr)
			{
				UploadedShaderProgram* shaderProgram = GetUploadedShaderProgram(item.shaderId);
				const UploadedTexture* uploadedTexture = GetUploadedTexture(item.textureId);
				IPipelineState* meshPipelineState =
				    shaderProgram != nullptr && uploadedTexture != nullptr
				        ? GetShaderPipelineState(*shaderProgram, uploadedTexture->filterMode,
				                                 uploadedTexture->addressMode)
				        : m_shared->meshPSO.RawPtr();
				IShaderResourceBinding* meshSRB = shaderProgram != nullptr
				                                      ? GetShaderResourceBinding(*shaderProgram, item.textureId)
				                                      : m_shared->meshSRB.RawPtr();
				if (shaderProgram != nullptr && meshSRB == nullptr)
				{
					NENE_LOG_WARN("DiligentDX12Adapter: shader {} requires texture {}, falling back to default mesh "
					              "pipeline for mesh {}",
					              item.shaderId.value, item.textureId.value, item.meshId.value);
					meshPipelineState = m_shared->meshPSO.RawPtr();
					meshSRB = m_shared->meshSRB.RawPtr();
				}

				if (meshPipelineState == nullptr || m_shared->meshConstantBuffer == nullptr)
				{
					NENE_LOG_WARN("DiligentDX12Adapter: mesh pipeline resources are missing for mesh {}",
					              item.meshId.value);
					continue;
				}

				PVoid mappedData = nullptr;
				m_shared->immediateContext->MapBuffer(m_shared->meshConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD,
				                                      mappedData);
				if (mappedData == nullptr)
				{
					NENE_LOG_WARN("DiligentDX12Adapter: failed to map mesh constant buffer");
					continue;
				}

				auto* drawConstants = static_cast<PrimitiveDrawConstants*>(mappedData);
				drawConstants->modelViewProjectionMatrix = item.modelViewProjectionMatrix;
				drawConstants->tint = item.tint;
				m_shared->immediateContext->UnmapBuffer(m_shared->meshConstantBuffer, MAP_WRITE);

				IBuffer* vertexBuffers[] = {uploadedMesh->vertexBuffer.RawPtr()};
				Uint64 offsets[] = {0};

				m_shared->immediateContext->SetPipelineState(meshPipelineState);
				m_shared->immediateContext->SetVertexBuffers(0, 1, vertexBuffers, offsets,
				                                             RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
				                                             SET_VERTEX_BUFFERS_FLAG_RESET);
				m_shared->immediateContext->SetIndexBuffer(uploadedMesh->indexBuffer, 0,
				                                           RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
				if (meshSRB != nullptr)
					m_shared->immediateContext->CommitShaderResources(meshSRB,
					                                                  RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

				DrawIndexedAttribs drawIndexedAttrs{uploadedMesh->indexCount, VT_UINT32, DRAW_FLAG_VERIFY_ALL};
				m_shared->immediateContext->DrawIndexed(drawIndexedAttrs);

				NENE_LOG_DEBUG("DiligentDX12Adapter: drew uploaded mesh={} material={} shader={} indices={} "
				               "texture={} tint=({:.2f}, {:.2f}, {:.2f}, {:.2f})",
				               item.meshId.value, item.materialId.value, item.shaderId.value, uploadedMesh->indexCount,
				               item.textureId.value, item.tint.r, item.tint.g, item.tint.b, item.tint.a);
			}
			else
			{
				// Built-in primitives are generated in the shader from SV_VertexID and need no vertex buffer.
				const size_t primitiveIndex = static_cast<size_t>(item.primitiveType);
				auto* pipelineState = GetPipelineState(item.primitiveType);
				auto* constantBuffer = m_shared->primitiveConstantBuffers[primitiveIndex].RawPtr();
				auto* srb = m_shared->primitiveSRBs[primitiveIndex].RawPtr();

				if (pipelineState == nullptr || constantBuffer == nullptr)
				{
					NENE_LOG_WARN("DiligentDX12Adapter: resources are missing for primitive type {}",
					              static_cast<int>(item.primitiveType));
					continue;
				}

				PVoid mappedData = nullptr;
				m_shared->immediateContext->MapBuffer(constantBuffer, MAP_WRITE, MAP_FLAG_DISCARD, mappedData);
				if (mappedData == nullptr)
				{
					NENE_LOG_WARN("DiligentDX12Adapter: failed to map constant buffer");
					continue;
				}

				auto* drawConstants = static_cast<PrimitiveDrawConstants*>(mappedData);
				drawConstants->modelViewProjectionMatrix = item.modelViewProjectionMatrix;
				drawConstants->tint = item.tint;
				m_shared->immediateContext->UnmapBuffer(constantBuffer, MAP_WRITE);

				m_shared->immediateContext->SetPipelineState(pipelineState);
				if (srb != nullptr)
					m_shared->immediateContext->CommitShaderResources(srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

				DrawAttribs drawAttrs{};
				drawAttrs.NumVertices = GetVertexCount(item.primitiveType);
				drawAttrs.StartVertexLocation = 0;

				m_shared->immediateContext->Draw(drawAttrs);

				NENE_LOG_DEBUG("DiligentDX12Adapter: drew primitive={} mesh={} material={} shader={} tint=({:.2f}, "
				               "{:.2f}, {:.2f}, {:.2f})",
				               static_cast<int>(item.primitiveType), item.meshId.value, item.materialId.value,
				               item.shaderId.value, item.tint.r, item.tint.g, item.tint.b, item.tint.a);
			}
		}
	}

	void DiligentDX12Adapter::Present(uint32_t syncInterval)
	{
		if (m_pSwapChain) m_pSwapChain->Present(syncInterval);
	}

	void DiligentDX12Adapter::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;

		if (m_shared->immediateContext) m_shared->immediateContext->Flush();

		if (m_pSwapChain)
		{
			m_pSwapChain->Resize(width, height);
			NENE_LOG_INFO("DiligentDX12Adapter: swap chain resized to {}x{}", width, height);
		}
	}

	void DiligentDX12Adapter::SetClearColor(const glm::vec4& color)
	{
		m_clearColor = color;
		NENE_LOG_INFO("DiligentDX12Adapter: clear color set to ({:.2f}, {:.2f}, {:.2f}, {:.2f})", m_clearColor.r,
		              m_clearColor.g, m_clearColor.b, m_clearColor.a);
	}

	// Create built-in primitive pipelines plus the fallback mesh pipeline.
	bool DiligentDX12Adapter::CreateResources()
	{
		const auto createPipelineState =
		    [this](PrimitiveType primitiveType, const char* name, const char* vertexShaderSource)
		{
			static const char* pixelShaderSource = R"raw(
                cbuffer Constants
                {
                    float4x4 ModelViewProjection;
                    float4 Tint;
                };

                struct PSInput
                {
                    float4 Pos   : SV_POSITION;
                    float4 Color : COLOR;
                };

                struct PSOutput
                {
                    float4 Color : SV_TARGET;
                };

                void main(in PSInput PSIn, out PSOutput PSOut)
                {
                    PSOut.Color = PSIn.Color;
                }
            )raw";

			GraphicsPipelineStateCreateInfo primitivePsoCreateInfo{};
			primitivePsoCreateInfo.PSODesc.Name = name;
			primitivePsoCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

			primitivePsoCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
			primitivePsoCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
			primitivePsoCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
			primitivePsoCreateInfo.GraphicsPipeline.PrimitiveTopology =
			    primitiveType == PrimitiveType::Line || primitiveType == PrimitiveType::DebugLine
			        ? PRIMITIVE_TOPOLOGY_LINE_LIST
			        : PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			primitivePsoCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
			// Debug lines must stay visible even where they coincide with or sit behind scene geometry.
			const bool depthEnabled = primitiveType != PrimitiveType::DebugLine;
			primitivePsoCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = depthEnabled;
			primitivePsoCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = depthEnabled;
			primitivePsoCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;

			ShaderResourceVariableDesc variables[] = {
			    {SHADER_TYPE_VERTEX, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};

			primitivePsoCreateInfo.PSODesc.ResourceLayout.Variables = variables;
			primitivePsoCreateInfo.PSODesc.ResourceLayout.NumVariables = 1;

			ShaderCreateInfo shaderCreateInfo{};
			shaderCreateInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
			shaderCreateInfo.Desc.UseCombinedTextureSamplers = true;

			RefCntAutoPtr<IShader> pVS;
			RefCntAutoPtr<IShader> pPS;

			shaderCreateInfo.Desc.ShaderType = SHADER_TYPE_VERTEX;
			shaderCreateInfo.Desc.Name = name;
			shaderCreateInfo.Source = vertexShaderSource;
			m_shared->device->CreateShader(shaderCreateInfo, &pVS);

			shaderCreateInfo.Desc.ShaderType = SHADER_TYPE_PIXEL;
			shaderCreateInfo.Desc.Name = "Primitive PS";
			shaderCreateInfo.Source = pixelShaderSource;
			m_shared->device->CreateShader(shaderCreateInfo, &pPS);

			if (!pVS || !pPS)
			{
				NENE_LOG_ERROR("Failed to create shaders for primitive pipeline '{}'", name);
				return false;
			}

			primitivePsoCreateInfo.pVS = pVS;
			primitivePsoCreateInfo.pPS = pPS;

			const size_t primitiveIndex = static_cast<size_t>(primitiveType);

			m_shared->device->CreateGraphicsPipelineState(primitivePsoCreateInfo,
			                                              &m_shared->primitivePSOs[primitiveIndex]);

			if (!m_shared->primitivePSOs[primitiveIndex])
			{
				NENE_LOG_ERROR("Failed to create Graphics Pipeline State '{}'", name);
				return false;
			}

			BufferDesc constantBufferDesc{};
			constantBufferDesc.Name = "Primitive Draw Constants";
			constantBufferDesc.Size = sizeof(PrimitiveDrawConstants);
			constantBufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
			constantBufferDesc.Usage = USAGE_DYNAMIC;
			constantBufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

			m_shared->device->CreateBuffer(constantBufferDesc, nullptr,
			                               &m_shared->primitiveConstantBuffers[primitiveIndex]);
			if (!m_shared->primitiveConstantBuffers[primitiveIndex])
			{
				NENE_LOG_ERROR("Failed to create constant buffer for '{}'", name);
				return false;
			}

			auto* constantsVariable =
			    m_shared->primitivePSOs[primitiveIndex]->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
			if (constantsVariable == nullptr)
			{
				NENE_LOG_ERROR("Failed to get shader constant variable for '{}'", name);
				return false;
			}

			constantsVariable->Set(m_shared->primitiveConstantBuffers[primitiveIndex]);
			m_shared->primitivePSOs[primitiveIndex]->CreateShaderResourceBinding(
			    &m_shared->primitiveSRBs[primitiveIndex], true);
			if (!m_shared->primitiveSRBs[primitiveIndex])
			{
				NENE_LOG_ERROR("Failed to create shader resource binding for '{}'", name);
				return false;
			}

			return true;
		};

		static const char* lineVertexShaderSource = R"raw(
            cbuffer Constants
            {
                float4x4 ModelViewProjection;
                float4 Tint;
            };

            struct PSInput
            {
                float4 Pos   : SV_POSITION;
                float4 Color : COLOR;
            };

            void main(in uint VertId : SV_VertexID, out PSInput PSIn)
            {
                float4 Pos[2] = {
                    float4(-0.5f, 0.0f, 0.0f, 1.0f),
                    float4( 0.5f, 0.0f, 0.0f, 1.0f)
                };

                PSIn.Pos = mul(ModelViewProjection, Pos[VertId]);
                PSIn.Color = float4(1.0f, 1.0f, 1.0f, 1.0f) * Tint;
            }
        )raw";

		static const char* meshVertexShaderSource = R"raw(
            cbuffer Constants
            {
                float4x4 ModelViewProjection;
                float4 Tint;
            };

            struct VSInput
            {
                float3 Pos    : ATTRIB0;
                float3 Normal : ATTRIB1;
                float2 UV     : ATTRIB2;
            };

            struct PSInput
            {
                float4 Pos    : SV_POSITION;
                float3 Normal : NORMAL;
                float2 UV     : TEXCOORD0;
                float4 Color  : COLOR;
            };

            void main(in VSInput VSIn, out PSInput PSIn)
            {
                PSIn.Pos = mul(ModelViewProjection, float4(VSIn.Pos, 1.0f));
                PSIn.Normal = VSIn.Normal;
                PSIn.UV = VSIn.UV;
                float3 vertexColor = float3(VSIn.UV.y < 0.5f ? 1.0f - VSIn.UV.x : 0.0f,
                                            VSIn.UV.y,
                                            VSIn.UV.y < 0.5f ? VSIn.UV.x : 0.0f);
                PSIn.Color = float4(vertexColor, 1.0f) * Tint;
            }
        )raw";

		static const char* meshPixelShaderSource = R"raw(
            struct PSInput
            {
                float4 Pos    : SV_POSITION;
                float3 Normal : NORMAL;
                float2 UV     : TEXCOORD0;
                float4 Color  : COLOR;
            };

            struct PSOutput
            {
                float4 Color : SV_TARGET;
            };

            void main(in PSInput PSIn, out PSOutput PSOut)
            {
                PSOut.Color = PSIn.Color;
            }
        )raw";

		static const char* triangleVertexShaderSource = R"raw(
            cbuffer Constants
            {
                float4x4 ModelViewProjection;
                float4 Tint;
            };

            struct PSInput
            {
                float4 Pos   : SV_POSITION;
                float4 Color : COLOR;
            };

            void main(in uint VertId : SV_VertexID, out PSInput PSIn)
            {
                float4 Pos[3] = {
                    float4(-0.5f, -0.5f, 0.0f, 1.0f),
                    float4( 0.0f,  0.5f, 0.0f, 1.0f),
                    float4( 0.5f, -0.5f, 0.0f, 1.0f)
                };
                float3 Col[3] = {
                    float3(1.0f, 0.0f, 0.0f),
                    float3(0.0f, 1.0f, 0.0f),
                    float3(0.0f, 0.0f, 1.0f)
                };

                PSIn.Pos = mul(ModelViewProjection, Pos[VertId]);
                PSIn.Color = float4(Col[VertId], 1.0f) * Tint;
            }
        )raw";

		static const char* quadVertexShaderSource = R"raw(
            cbuffer Constants
            {
                float4x4 ModelViewProjection;
                float4 Tint;
            };

            struct PSInput
            {
                float4 Pos   : SV_POSITION;
                float4 Color : COLOR;
            };

            void main(in uint VertId : SV_VertexID, out PSInput PSIn)
            {
                float4 Pos[6] = {
                    float4(-0.5f, -0.5f, 0.0f, 1.0f),
                    float4(-0.5f,  0.5f, 0.0f, 1.0f),
                    float4( 0.5f,  0.5f, 0.0f, 1.0f),
                    float4(-0.5f, -0.5f, 0.0f, 1.0f),
                    float4( 0.5f,  0.5f, 0.0f, 1.0f),
                    float4( 0.5f, -0.5f, 0.0f, 1.0f)
                };

                PSIn.Pos = mul(ModelViewProjection, Pos[VertId]);
                PSIn.Color = float4(0.9f, 0.8f, 0.2f, 1.0f) * Tint;
            }
        )raw";

		static const char* cubeVertexShaderSource = R"raw(
            cbuffer Constants
            {
                float4x4 ModelViewProjection;
                float4 Tint;
            };

            struct PSInput
            {
                float4 Pos   : SV_POSITION;
                float4 Color : COLOR;
            };

            void main(in uint VertId : SV_VertexID, out PSInput PSIn)
            {
                float4 Pos[36] = {
                    float4(-0.4f, -0.4f,  0.4f, 1.0f), float4(-0.4f,  0.4f,  0.4f, 1.0f), float4( 0.4f,  0.4f,  0.4f, 1.0f),
                    float4(-0.4f, -0.4f,  0.4f, 1.0f), float4( 0.4f,  0.4f,  0.4f, 1.0f), float4( 0.4f, -0.4f,  0.4f, 1.0f),

                    float4(-0.4f, -0.4f, -0.4f, 1.0f), float4( 0.4f,  0.4f, -0.4f, 1.0f), float4(-0.4f,  0.4f, -0.4f, 1.0f),
                    float4(-0.4f, -0.4f, -0.4f, 1.0f), float4( 0.4f, -0.4f, -0.4f, 1.0f), float4( 0.4f,  0.4f, -0.4f, 1.0f),

                    float4(-0.4f, -0.4f, -0.4f, 1.0f), float4(-0.4f,  0.4f,  0.4f, 1.0f), float4(-0.4f,  0.4f, -0.4f, 1.0f),
                    float4(-0.4f, -0.4f, -0.4f, 1.0f), float4(-0.4f, -0.4f,  0.4f, 1.0f), float4(-0.4f,  0.4f,  0.4f, 1.0f),

                    float4( 0.4f, -0.4f, -0.4f, 1.0f), float4( 0.4f,  0.4f, -0.4f, 1.0f), float4( 0.4f,  0.4f,  0.4f, 1.0f),
                    float4( 0.4f, -0.4f, -0.4f, 1.0f), float4( 0.4f,  0.4f,  0.4f, 1.0f), float4( 0.4f, -0.4f,  0.4f, 1.0f),

                    float4(-0.4f,  0.4f, -0.4f, 1.0f), float4(-0.4f,  0.4f,  0.4f, 1.0f), float4( 0.4f,  0.4f,  0.4f, 1.0f),
                    float4(-0.4f,  0.4f, -0.4f, 1.0f), float4( 0.4f,  0.4f,  0.4f, 1.0f), float4( 0.4f,  0.4f, -0.4f, 1.0f),

                    float4(-0.4f, -0.4f, -0.4f, 1.0f), float4( 0.4f, -0.4f,  0.4f, 1.0f), float4(-0.4f, -0.4f,  0.4f, 1.0f),
                    float4(-0.4f, -0.4f, -0.4f, 1.0f), float4( 0.4f, -0.4f, -0.4f, 1.0f), float4( 0.4f, -0.4f,  0.4f, 1.0f)
                };

                PSIn.Pos = mul(ModelViewProjection, Pos[VertId]);
                PSIn.Color = float4(0.6f, 0.8f, 1.0f, 1.0f) * Tint;
            }
        )raw";

		if (!createPipelineState(PrimitiveType::Line, "Simple Line PSO", lineVertexShaderSource) ||
		    !createPipelineState(PrimitiveType::DebugLine, "Debug Line PSO", lineVertexShaderSource) ||
		    !createPipelineState(PrimitiveType::Triangle, "Simple Triangle PSO", triangleVertexShaderSource) ||
		    !createPipelineState(PrimitiveType::Quad, "Simple Quad PSO", quadVertexShaderSource) ||
		    !createPipelineState(PrimitiveType::Cube, "Simple Cube PSO", cubeVertexShaderSource))
		{
			return false;
		}

		GraphicsPipelineStateCreateInfo meshPSOCreateInfo{};
		meshPSOCreateInfo.PSODesc.Name = "Mesh PSO";
		meshPSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

		meshPSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
		meshPSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
		meshPSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
		meshPSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		meshPSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
		meshPSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
		meshPSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
		meshPSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;

		LayoutElement meshLayoutElements[] = {LayoutElement{0, 0, 3, VT_FLOAT32, false},
		                                      LayoutElement{1, 0, 3, VT_FLOAT32, false},
		                                      LayoutElement{2, 0, 2, VT_FLOAT32, false}};
		meshPSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = meshLayoutElements;
		meshPSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(meshLayoutElements);

		ShaderResourceVariableDesc meshVariables[] = {
		    {SHADER_TYPE_VERTEX, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};

		meshPSOCreateInfo.PSODesc.ResourceLayout.Variables = meshVariables;
		meshPSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(meshVariables);

		ShaderCreateInfo meshShaderCI{};
		meshShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
		meshShaderCI.Desc.UseCombinedTextureSamplers = true;

		RefCntAutoPtr<IShader> meshVS;
		RefCntAutoPtr<IShader> meshPS;

		meshShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
		meshShaderCI.Desc.Name = "Mesh VS";
		meshShaderCI.Source = meshVertexShaderSource;
		m_shared->device->CreateShader(meshShaderCI, &meshVS);

		meshShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
		meshShaderCI.Desc.Name = "Mesh PS";
		meshShaderCI.Source = meshPixelShaderSource;
		m_shared->device->CreateShader(meshShaderCI, &meshPS);

		if (!meshVS || !meshPS)
		{
			NENE_LOG_ERROR("Failed to create mesh shaders");
			return false;
		}

		meshPSOCreateInfo.pVS = meshVS;
		meshPSOCreateInfo.pPS = meshPS;

		m_shared->device->CreateGraphicsPipelineState(meshPSOCreateInfo, &m_shared->meshPSO);
		if (!m_shared->meshPSO)
		{
			NENE_LOG_ERROR("Failed to create mesh graphics pipeline state");
			return false;
		}

		BufferDesc meshConstantBufferDesc{};
		meshConstantBufferDesc.Name = "Mesh Draw Constants";
		meshConstantBufferDesc.Size = sizeof(PrimitiveDrawConstants);
		meshConstantBufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
		meshConstantBufferDesc.Usage = USAGE_DYNAMIC;
		meshConstantBufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

		m_shared->device->CreateBuffer(meshConstantBufferDesc, nullptr, &m_shared->meshConstantBuffer);
		if (!m_shared->meshConstantBuffer)
		{
			NENE_LOG_ERROR("Failed to create mesh constant buffer");
			return false;
		}

		auto* meshConstantsVariable = m_shared->meshPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
		if (meshConstantsVariable == nullptr)
		{
			NENE_LOG_ERROR("Failed to get mesh constant buffer variable");
			return false;
		}

		meshConstantsVariable->Set(m_shared->meshConstantBuffer);
		m_shared->meshPSO->CreateShaderResourceBinding(&m_shared->meshSRB, true);
		if (!m_shared->meshSRB)
		{
			NENE_LOG_ERROR("Failed to create mesh shader resource binding");
			return false;
		}

		return true;
	}

	IPipelineState* DiligentDX12Adapter::GetPipelineState(PrimitiveType primitiveType) const
	{
		return m_shared->primitivePSOs[static_cast<size_t>(primitiveType)];
	}

	const DiligentDX12Adapter::UploadedMeshBuffers* DiligentDX12Adapter::GetUploadedMesh(MeshId meshId) const
	{
		if (!meshId.IsValid()) return nullptr;

		const auto it = m_shared->uploadedMeshes.find(meshId.value);
		return it != m_shared->uploadedMeshes.end() ? &it->second : nullptr;
	}

	const DiligentDX12Adapter::UploadedTexture* DiligentDX12Adapter::GetUploadedTexture(TextureId textureId) const
	{
		if (!textureId.IsValid()) return nullptr;

		const auto it = m_shared->uploadedTextures.find(textureId.value);
		return it != m_shared->uploadedTextures.end() ? &it->second : nullptr;
	}

	DiligentDX12Adapter::UploadedShaderProgram* DiligentDX12Adapter::GetUploadedShaderProgram(ShaderId shaderId)
	{
		if (!shaderId.IsValid()) return nullptr;

		const auto it = m_shared->uploadedShaderPrograms.find(shaderId.value);
		return it != m_shared->uploadedShaderPrograms.end() ? &it->second : nullptr;
	}

	IPipelineState* DiligentDX12Adapter::GetShaderPipelineState(UploadedShaderProgram& shaderProgram,
	                                                            TextureFilterMode filterMode,
	                                                            TextureAddressMode addressMode) const
	{
		if (addressMode == TextureAddressMode::Clamp)
			return filterMode == TextureFilterMode::Nearest ? shaderProgram.nearestClampPipelineState.RawPtr()
			                                                : shaderProgram.linearClampPipelineState.RawPtr();

		return filterMode == TextureFilterMode::Nearest ? shaderProgram.nearestWrapPipelineState.RawPtr()
		                                                : shaderProgram.linearWrapPipelineState.RawPtr();
	}

	IShaderResourceBinding* DiligentDX12Adapter::GetShaderResourceBinding(UploadedShaderProgram& shaderProgram,
	                                                                      TextureId textureId)
	{
		if (!textureId.IsValid()) return nullptr;

		if (const auto cached = shaderProgram.srbsByTexture.find(textureId.value);
		    cached != shaderProgram.srbsByTexture.end())
			return cached->second.RawPtr();

		const UploadedTexture* texture = GetUploadedTexture(textureId);
		if (texture == nullptr || texture->shaderResourceView == nullptr) return nullptr;

		IPipelineState* pipelineState =
		    GetShaderPipelineState(shaderProgram, texture->filterMode, texture->addressMode);
		if (pipelineState == nullptr) return nullptr;

		// SRBs are cached per texture because the mutable texture binding differs for each material.
		RefCntAutoPtr<IShaderResourceBinding> srb;
		pipelineState->CreateShaderResourceBinding(&srb, true);
		if (srb == nullptr) return nullptr;

		if (auto* textureVariable = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture"); textureVariable != nullptr)
			textureVariable->Set(texture->shaderResourceView);

		auto [inserted, _] = shaderProgram.srbsByTexture.emplace(textureId.value, srb);
		return inserted->second.RawPtr();
	}

	uint32_t DiligentDX12Adapter::GetVertexCount(PrimitiveType primitiveType) const
	{
		switch (primitiveType)
		{
		case PrimitiveType::Line:
		case PrimitiveType::DebugLine:
			return 2;
		case PrimitiveType::Triangle:
			return 3;
		case PrimitiveType::Quad:
			return 6;
		case PrimitiveType::Cube:
			return 36;
		default:
			return 3;
		}
	}

} // namespace NeneEngine

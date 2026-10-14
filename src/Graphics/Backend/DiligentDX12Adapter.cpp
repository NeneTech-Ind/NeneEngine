// DiligentDX12Adapter.cpp

#include "Graphics/Backend/DiligentDX12Adapter.h"

#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/Shader.h"
#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "../external/DiligentEngine/DiligentTools/TextureLoader/interface/TextureLoader.h"

#include "Core/NeneLogger.h"

#include <memory>
#include <string>
#include <string_view>

namespace NeneEngine
{

	using namespace Diligent;

	namespace
	{
#ifdef NDEBUG
		constexpr bool kEnableGpuValidation = false;
		constexpr DRAW_FLAGS kDrawVerifyFlags = DRAW_FLAG_NONE;
#else
		constexpr bool kEnableGpuValidation = true;
		constexpr DRAW_FLAGS kDrawVerifyFlags = DRAW_FLAG_VERIFY_ALL;
#endif

		struct DrawConstants
		{
			glm::mat4 modelViewProjectionMatrix = glm::mat4(1.0f);
			glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
		};

		constexpr std::string_view kDrawConstantsHlsl = R"raw(
            cbuffer Constants
            {
                float4x4 ModelViewProjection;
                float4 Tint;
            };
        )raw";

		constexpr std::string_view kColorVertexOutputHlsl = R"raw(
            struct PSInput
            {
                float4 Pos   : SV_POSITION;
                float4 Color : COLOR;
            };
        )raw";

		constexpr std::string_view kMeshVertexOutputHlsl = R"raw(
            struct PSInput
            {
                float4 Pos    : SV_POSITION;
                float3 Normal : NORMAL;
                float2 UV     : TEXCOORD0;
                float4 Color  : COLOR;
            };
        )raw";

		constexpr std::string_view kPassThroughPixelShaderHlsl = R"raw(
            struct PSOutput
            {
                float4 Color : SV_TARGET;
            };

            void main(in PSInput PSIn, out PSOutput PSOut)
            {
                PSOut.Color = PSIn.Color;
            }
        )raw";

		// Built-in primitives are generated in the shader from SV_VertexID and need no vertex buffer.
		constexpr const char* kLineVertexShaderBody = R"raw(
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

		constexpr const char* kTriangleVertexShaderBody = R"raw(
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

		constexpr const char* kQuadVertexShaderBody = R"raw(
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

		constexpr const char* kCubeVertexShaderBody = R"raw(
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

		constexpr std::string_view kMeshVertexShaderBody = R"raw(
            struct VSInput
            {
                float3 Pos    : ATTRIB0;
                float3 Normal : ATTRIB1;
                float2 UV     : ATTRIB2;
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

		const LayoutElement kMeshLayoutElements[] = {LayoutElement{0, 0, 3, VT_FLOAT32, false},
		                                             LayoutElement{1, 0, 3, VT_FLOAT32, false},
		                                             LayoutElement{2, 0, 2, VT_FLOAT32, false}};

		const ShaderResourceVariableDesc kDrawConstantsVariables[] = {
		    {SHADER_TYPE_VERTEX, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};

		const ShaderResourceVariableDesc kTexturedMeshVariables[] = {
		    {SHADER_TYPE_VERTEX, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
		    {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};

		std::string ComposeHlsl(std::initializer_list<std::string_view> parts)
		{
			std::string source;
			for (const std::string_view part : parts) source += part;
			return source;
		}

		struct ShaderPair
		{
			RefCntAutoPtr<IShader> vertexShader;
			RefCntAutoPtr<IShader> pixelShader;

			[[nodiscard]] bool IsValid() const { return vertexShader && pixelShader; }
		};

		ShaderPair CreateShaderPair(IRenderDevice& device, const char* vertexName, const char* vertexSource,
		                            const char* pixelName, const char* pixelSource)
		{
			ShaderCreateInfo shaderCreateInfo{};
			shaderCreateInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
			shaderCreateInfo.Desc.UseCombinedTextureSamplers = true;

			ShaderPair shaders;

			shaderCreateInfo.Desc.ShaderType = SHADER_TYPE_VERTEX;
			shaderCreateInfo.Desc.Name = vertexName;
			shaderCreateInfo.Source = vertexSource;
			device.CreateShader(shaderCreateInfo, &shaders.vertexShader);

			shaderCreateInfo.Desc.ShaderType = SHADER_TYPE_PIXEL;
			shaderCreateInfo.Desc.Name = pixelName;
			shaderCreateInfo.Source = pixelSource;
			device.CreateShader(shaderCreateInfo, &shaders.pixelShader);

			return shaders;
		}

		void ConfigureGraphicsPipeline(GraphicsPipelineStateCreateInfo& createInfo, const char* name,
		                               TEXTURE_FORMAT colorFormat, TEXTURE_FORMAT depthFormat,
		                               PRIMITIVE_TOPOLOGY topology, bool depthEnabled)
		{
			createInfo.PSODesc.Name = name;
			createInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

			auto& pipeline = createInfo.GraphicsPipeline;
			pipeline.NumRenderTargets = 1;
			pipeline.RTVFormats[0] = colorFormat;
			pipeline.DSVFormat = depthFormat;
			pipeline.PrimitiveTopology = topology;
			pipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
			pipeline.DepthStencilDesc.DepthEnable = depthEnabled;
			pipeline.DepthStencilDesc.DepthWriteEnable = depthEnabled;
			pipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;
		}

		void SetMeshInputLayout(GraphicsPipelineStateCreateInfo& createInfo)
		{
			createInfo.GraphicsPipeline.InputLayout.LayoutElements = kMeshLayoutElements;
			createInfo.GraphicsPipeline.InputLayout.NumElements = _countof(kMeshLayoutElements);
		}

		RefCntAutoPtr<IBuffer> CreateDrawConstantsBuffer(IRenderDevice& device, const char* name)
		{
			BufferDesc bufferDesc{};
			bufferDesc.Name = name;
			bufferDesc.Size = sizeof(DrawConstants);
			bufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
			bufferDesc.Usage = USAGE_DYNAMIC;
			bufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

			RefCntAutoPtr<IBuffer> buffer;
			device.CreateBuffer(bufferDesc, nullptr, &buffer);
			return buffer;
		}

		bool CreateDrawPipeline(IRenderDevice& device, const GraphicsPipelineStateCreateInfo& createInfo,
		                        const char* constantBufferName, DiligentDX12SharedDevice::DrawPipeline& pipeline)
		{
			const char* name = createInfo.PSODesc.Name;

			device.CreateGraphicsPipelineState(createInfo, &pipeline.pipelineState);
			if (!pipeline.pipelineState)
			{
				NENE_LOG_ERROR("Failed to create graphics pipeline state '{}'", name);
				return false;
			}

			pipeline.constantBuffer = CreateDrawConstantsBuffer(device, constantBufferName);
			if (!pipeline.constantBuffer)
			{
				NENE_LOG_ERROR("Failed to create constant buffer for '{}'", name);
				return false;
			}

			auto* constantsVariable = pipeline.pipelineState->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
			if (constantsVariable == nullptr)
			{
				NENE_LOG_ERROR("Failed to get shader constant variable for '{}'", name);
				return false;
			}

			constantsVariable->Set(pipeline.constantBuffer);
			pipeline.pipelineState->CreateShaderResourceBinding(&pipeline.shaderResourceBinding, true);
			if (!pipeline.shaderResourceBinding)
			{
				NENE_LOG_ERROR("Failed to create shader resource binding for '{}'", name);
				return false;
			}

			return true;
		}

		constexpr size_t GetSamplerVariantIndex(TextureFilterMode filterMode, TextureAddressMode addressMode)
		{
			return (addressMode == TextureAddressMode::Clamp ? 2 : 0) +
			       (filterMode == TextureFilterMode::Nearest ? 1 : 0);
		}

		SamplerDesc MakeSamplerDesc(TextureFilterMode filterMode, TextureAddressMode addressMode)
		{
			const FILTER_TYPE filterType =
			    filterMode == TextureFilterMode::Nearest ? FILTER_TYPE_POINT : FILTER_TYPE_LINEAR;
			const TEXTURE_ADDRESS_MODE diligentAddressMode =
			    addressMode == TextureAddressMode::Clamp ? TEXTURE_ADDRESS_CLAMP : TEXTURE_ADDRESS_WRAP;

			SamplerDesc samplerDesc{};
			samplerDesc.MinFilter = filterType;
			samplerDesc.MagFilter = filterType;
			samplerDesc.MipFilter = filterType;
			samplerDesc.AddressU = diligentAddressMode;
			samplerDesc.AddressV = diligentAddressMode;
			samplerDesc.AddressW = diligentAddressMode;
			return samplerDesc;
		}

		constexpr bool IsLinePrimitive(PrimitiveType primitiveType)
		{
			return primitiveType == PrimitiveType::Line || primitiveType == PrimitiveType::DebugLine;
		}

		constexpr uint32_t GetPrimitiveVertexCount(PrimitiveType primitiveType)
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

		template <typename Map> auto* FindUploaded(Map& resources, uint32_t id)
		{
			const auto it = id != 0 ? resources.find(id) : resources.end();
			return it != resources.end() ? std::addressof(it->second) : nullptr;
		}
	} // namespace

	DiligentDX12SharedDevice::~DiligentDX12SharedDevice()
	{
		if (immediateContext) immediateContext->Flush();
		if (device) device->IdleGPU();
	}

	bool DiligentDX12SharedDevice::InitializeDevice(IEngineFactoryD3D12& factory)
	{
		if (device && immediateContext) return true;

		EngineD3D12CreateInfo engineCreateInfo{};
		engineCreateInfo.EnableValidation = kEnableGpuValidation;

		factory.CreateDeviceAndContextsD3D12(engineCreateInfo, &device, &immediateContext);
		if (!device || !immediateContext)
		{
			NENE_LOG_ERROR("Failed to create D3D12 device and immediate context");
			immediateContext.Release();
			device.Release();
			return false;
		}

		NENE_LOG_INFO("DiligentDX12Adapter: created shared D3D12 device (validation={})", kEnableGpuValidation);
		return true;
	}

	bool DiligentDX12SharedDevice::InitializePipelines(TEXTURE_FORMAT colorFormat, TEXTURE_FORMAT depthFormat)
	{
		if (pipelinesCreated)
		{
			if (colorFormat == colorBufferFormat && depthFormat == depthBufferFormat) return true;

			NENE_LOG_ERROR("DiligentDX12Adapter: swap chain formats do not match the shared pipelines");
			return false;
		}

		colorBufferFormat = colorFormat;
		depthBufferFormat = depthFormat;

		if (!CreatePrimitivePipeline(PrimitiveType::Line, "Simple Line PSO", kLineVertexShaderBody) ||
		    !CreatePrimitivePipeline(PrimitiveType::DebugLine, "Debug Line PSO", kLineVertexShaderBody) ||
		    !CreatePrimitivePipeline(PrimitiveType::Triangle, "Simple Triangle PSO", kTriangleVertexShaderBody) ||
		    !CreatePrimitivePipeline(PrimitiveType::Quad, "Simple Quad PSO", kQuadVertexShaderBody) ||
		    !CreatePrimitivePipeline(PrimitiveType::Cube, "Simple Cube PSO", kCubeVertexShaderBody) ||
		    !CreateMeshPipeline())
		{
			NENE_LOG_ERROR("Failed to create D3D12 render resources");
			return false;
		}

		pipelinesCreated = true;
		return true;
	}

	bool DiligentDX12SharedDevice::CreatePrimitivePipeline(PrimitiveType primitiveType, const char* name,
	                                                       const char* vertexShaderBody)
	{
		const std::string vertexSource = ComposeHlsl({kDrawConstantsHlsl, kColorVertexOutputHlsl, vertexShaderBody});
		const std::string pixelSource = ComposeHlsl({kColorVertexOutputHlsl, kPassThroughPixelShaderHlsl});
		const ShaderPair shaders =
		    CreateShaderPair(*device, name, vertexSource.c_str(), "Primitive PS", pixelSource.c_str());
		if (!shaders.IsValid())
		{
			NENE_LOG_ERROR("Failed to create shaders for primitive pipeline '{}'", name);
			return false;
		}

		// Debug lines must stay visible even where they coincide with or sit behind scene geometry.
		const bool depthEnabled = primitiveType != PrimitiveType::DebugLine;
		const PRIMITIVE_TOPOLOGY topology =
		    IsLinePrimitive(primitiveType) ? PRIMITIVE_TOPOLOGY_LINE_LIST : PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		GraphicsPipelineStateCreateInfo createInfo{};
		ConfigureGraphicsPipeline(createInfo, name, colorBufferFormat, depthBufferFormat, topology, depthEnabled);
		createInfo.PSODesc.ResourceLayout.Variables = kDrawConstantsVariables;
		createInfo.PSODesc.ResourceLayout.NumVariables = _countof(kDrawConstantsVariables);
		createInfo.pVS = shaders.vertexShader;
		createInfo.pPS = shaders.pixelShader;

		return CreateDrawPipeline(*device, createInfo, "Primitive Draw Constants",
		                          primitivePipelines[static_cast<size_t>(primitiveType)]);
	}

	bool DiligentDX12SharedDevice::CreateMeshPipeline()
	{
		const std::string vertexSource =
		    ComposeHlsl({kDrawConstantsHlsl, kMeshVertexOutputHlsl, kMeshVertexShaderBody});
		const std::string pixelSource = ComposeHlsl({kMeshVertexOutputHlsl, kPassThroughPixelShaderHlsl});
		const ShaderPair shaders =
		    CreateShaderPair(*device, "Mesh VS", vertexSource.c_str(), "Mesh PS", pixelSource.c_str());
		if (!shaders.IsValid())
		{
			NENE_LOG_ERROR("Failed to create mesh shaders");
			return false;
		}

		GraphicsPipelineStateCreateInfo createInfo{};
		ConfigureGraphicsPipeline(createInfo, "Mesh PSO", colorBufferFormat, depthBufferFormat,
		                          PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true);
		SetMeshInputLayout(createInfo);
		createInfo.PSODesc.ResourceLayout.Variables = kDrawConstantsVariables;
		createInfo.PSODesc.ResourceLayout.NumVariables = _countof(kDrawConstantsVariables);
		createInfo.pVS = shaders.vertexShader;
		createInfo.pPS = shaders.pixelShader;

		return CreateDrawPipeline(*device, createInfo, "Mesh Draw Constants", meshPipeline);
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

		IEngineFactoryD3D12* factory = LoadAndGetEngineFactoryD3D12();
		if (factory == nullptr)
		{
			NENE_LOG_ERROR("Failed to load D3D12 Engine Factory");
			return false;
		}

		if (!m_shared->InitializeDevice(*factory)) return false;

		SwapChainDesc swapChainCreateDesc{};
		swapChainCreateDesc.Width = width;
		swapChainCreateDesc.Height = height;
		swapChainCreateDesc.BufferCount = 2; // Double buffering
		swapChainCreateDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;

		factory->CreateSwapChainD3D12(m_shared->device, m_shared->immediateContext, swapChainCreateDesc,
		                              FullScreenModeDesc{}, Win32NativeWindow{hwnd}, &m_swapChain);
		if (!m_swapChain)
		{
			NENE_LOG_ERROR("Failed to create D3D12 swap chain");
			return false;
		}

		const auto& swapChainDesc = m_swapChain->GetDesc();
		NENE_LOG_INFO("DiligentDX12Adapter: swap chain formats color={} depth={}",
		              static_cast<int>(swapChainDesc.ColorBufferFormat),
		              static_cast<int>(swapChainDesc.DepthBufferFormat));

		if (!m_shared->InitializePipelines(swapChainDesc.ColorBufferFormat, swapChainDesc.DepthBufferFormat))
		{
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
		m_swapChain.Release();
	}

	uintptr_t DiligentDX12Adapter::GetResourceDomainKey() const
	{
		return reinterpret_cast<uintptr_t>(m_shared.get());
	}

	bool DiligentDX12Adapter::IsDeviceReady(const char* operation) const
	{
		if (m_shared->device) return true;

		NENE_LOG_ERROR("DiligentDX12Adapter: {} called before device initialization", operation);
		return false;
	}

	GPUBufferId DiligentDX12Adapter::CreateImmutableBuffer(const char* name, Diligent::BIND_FLAGS bindFlags,
	                                                       const void* data, uint64_t sizeBytes)
	{
		BufferDesc bufferDesc{};
		bufferDesc.Name = name;
		bufferDesc.BindFlags = bindFlags;
		bufferDesc.Usage = USAGE_IMMUTABLE;
		bufferDesc.Size = sizeBytes;

		BufferData bufferData{};
		bufferData.pData = data;
		bufferData.DataSize = sizeBytes;

		RefCntAutoPtr<IBuffer> buffer;
		m_shared->device->CreateBuffer(bufferDesc, &bufferData, &buffer);
		if (!buffer) return {};

		const GPUBufferId bufferId{m_shared->nextBufferId++};
		m_shared->uploadedBuffers.emplace(bufferId.value, std::move(buffer));
		return bufferId;
	}

	GPUBuffer DiligentDX12Adapter::CreateVertexBuffer(const void* vertexData, uint64_t sizeBytes, uint32_t vertexCount)
	{
		if (!IsDeviceReady("CreateVertexBuffer")) return {};

		if (vertexData == nullptr || sizeBytes == 0 || vertexCount == 0)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: CreateVertexBuffer received empty data");
			return {};
		}

		const GPUBufferId bufferId =
		    CreateImmutableBuffer("Mesh Vertex Buffer", BIND_VERTEX_BUFFER, vertexData, sizeBytes);
		if (!bufferId.IsValid())
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: failed to create vertex buffer (bytes={}, vertices={})", sizeBytes,
			               vertexCount);
			return {};
		}

		return GPUBuffer{bufferId, sizeBytes, vertexCount};
	}

	GPUBuffer DiligentDX12Adapter::CreateIndexBuffer(const uint32_t* indices, uint32_t indexCount)
	{
		if (!IsDeviceReady("CreateIndexBuffer")) return {};

		if (indices == nullptr || indexCount == 0)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: CreateIndexBuffer received empty data");
			return {};
		}

		const uint64_t sizeBytes = static_cast<uint64_t>(indexCount) * sizeof(uint32_t);
		const GPUBufferId bufferId = CreateImmutableBuffer("Mesh Index Buffer", BIND_INDEX_BUFFER, indices, sizeBytes);
		if (!bufferId.IsValid())
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: failed to create index buffer (indices={})", indexCount);
			return {};
		}

		return GPUBuffer{bufferId, sizeBytes, indexCount};
	}

	GPUMesh DiligentDX12Adapter::UploadMesh(const MeshData& meshData)
	{
		if (meshData.vertices.empty() || meshData.indices.empty())
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: UploadMesh received empty mesh data");
			return {};
		}

		const auto vertexCount = static_cast<uint32_t>(meshData.vertices.size());
		const auto indexCount = static_cast<uint32_t>(meshData.indices.size());
		const GPUBuffer vertexBuffer =
		    CreateVertexBuffer(meshData.vertices.data(), meshData.vertices.size() * sizeof(Vertex), vertexCount);
		const GPUBuffer indexBuffer = CreateIndexBuffer(meshData.indices.data(), indexCount);
		if (!vertexBuffer.IsValid() || !indexBuffer.IsValid()) return {};

		UploadedMesh uploadedMesh{};
		uploadedMesh.vertexBuffer = *FindUploaded(m_shared->uploadedBuffers, vertexBuffer.bufferId.value);
		uploadedMesh.indexBuffer = *FindUploaded(m_shared->uploadedBuffers, indexBuffer.bufferId.value);
		uploadedMesh.indexCount = indexCount;

		const MeshId meshId{m_shared->nextMeshId++};
		m_shared->uploadedMeshes.emplace(meshId.value, std::move(uploadedMesh));

		NENE_LOG_INFO("DiligentDX12Adapter: uploaded mesh {} (vertices={}, indices={})", meshId.value, vertexCount,
		              indexCount);

		return GPUMesh{meshId, vertexBuffer.bufferId, indexBuffer.bufferId, vertexCount, indexCount};
	}

	GPUTexture DiligentDX12Adapter::CreateTexture2D(const TextureResource& texture)
	{
		if (!IsDeviceReady("CreateTexture2D")) return {};

		TextureLoadInfo loadInfo{};
		loadInfo.IsSRGB = texture.isSrgb;

		RefCntAutoPtr<ITextureLoader> textureLoader;
		CreateTextureLoaderFromFile(texture.path.c_str(), IMAGE_FILE_FORMAT_UNKNOWN, loadInfo, &textureLoader);
		if (!textureLoader)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: failed to create texture loader for '{}'", texture.path);
			return {};
		}

		DiligentDX12SharedDevice::UploadedTexture uploadedTexture{};
		textureLoader->CreateTexture(m_shared->device, &uploadedTexture.texture);
		if (!uploadedTexture.texture)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: failed to create GPU texture for '{}'", texture.path);
			return {};
		}

		uploadedTexture.filterMode = texture.filterMode;
		uploadedTexture.addressMode = texture.addressMode;
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

		const auto& textureDesc = uploadedTexture.texture->GetDesc();
		const TextureId textureId{m_shared->nextTextureId++};
		m_shared->uploadedTextures.emplace(textureId.value, std::move(uploadedTexture));
		NENE_LOG_INFO("DiligentDX12Adapter: uploaded texture {} '{}' ({}x{})", textureId.value, texture.path,
		              textureDesc.Width, textureDesc.Height);
		return GPUTexture{textureId, textureDesc.Width, textureDesc.Height};
	}

	GPUShaderProgram DiligentDX12Adapter::CreateShaderProgram(const ShaderProgramResource& shaderProgram)
	{
		if (!m_shared->device || !m_shared->pipelinesCreated)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: CreateShaderProgram called before renderer initialization");
			return {};
		}

		const ShaderPair shaders =
		    CreateShaderPair(*m_shared->device, shaderProgram.vertexPath.c_str(), shaderProgram.vertexSource.c_str(),
		                     shaderProgram.pixelPath.c_str(), shaderProgram.pixelSource.c_str());
		if (!shaders.IsValid())
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: failed to create shader program '{}', '{}'", shaderProgram.vertexPath,
			               shaderProgram.pixelPath);
			return {};
		}

		GraphicsPipelineStateCreateInfo createInfo{};
		ConfigureGraphicsPipeline(createInfo, "Resource Shader Program PSO", m_shared->colorBufferFormat,
		                          m_shared->depthBufferFormat, PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true);
		SetMeshInputLayout(createInfo);
		createInfo.PSODesc.ResourceLayout.Variables = kTexturedMeshVariables;
		createInfo.PSODesc.ResourceLayout.NumVariables = _countof(kTexturedMeshVariables);
		createInfo.pVS = shaders.vertexShader;
		createInfo.pPS = shaders.pixelShader;

		UploadedShaderProgram uploadedShader{};
		for (const TextureAddressMode addressMode : {TextureAddressMode::Wrap, TextureAddressMode::Clamp})
		{
			for (const TextureFilterMode filterMode : {TextureFilterMode::Linear, TextureFilterMode::Nearest})
			{
				const ImmutableSamplerDesc immutableSamplers[] = {
				    {SHADER_TYPE_PIXEL, "g_Texture", MakeSamplerDesc(filterMode, addressMode)}};
				createInfo.PSODesc.ResourceLayout.ImmutableSamplers = immutableSamplers;
				createInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(immutableSamplers);

				auto& pipelineState = uploadedShader.pipelineStates[GetSamplerVariantIndex(filterMode, addressMode)];
				m_shared->device->CreateGraphicsPipelineState(createInfo, &pipelineState);
				if (!pipelineState)
				{
					NENE_LOG_ERROR("DiligentDX12Adapter: failed to create shader program pipelines");
					return {};
				}

				if (auto* constantsVariable = pipelineState->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants"))
					constantsVariable->Set(m_shared->meshPipeline.constantBuffer);
			}
		}

		const ShaderId shaderId{m_shared->nextShaderId++};
		m_shared->uploadedShaderPrograms.emplace(shaderId.value, std::move(uploadedShader));
		NENE_LOG_INFO("DiligentDX12Adapter: created shader program {}", shaderId.value);
		return GPUShaderProgram{shaderId};
	}

	void DiligentDX12Adapter::BeginFrame()
	{
		if (!m_swapChain || !m_shared->immediateContext) return;

		ITextureView* renderTargetView = m_swapChain->GetCurrentBackBufferRTV();
		ITextureView* depthStencilView = m_swapChain->GetDepthBufferDSV();
		if (renderTargetView == nullptr)
		{
			NENE_LOG_WARN("DiligentDX12Adapter: current back buffer RTV is missing");
			return;
		}

		IDeviceContext& context = *m_shared->immediateContext;
		context.SetRenderTargets(1, &renderTargetView, depthStencilView, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		const float clearColor[] = {m_clearColor.r, m_clearColor.g, m_clearColor.b, m_clearColor.a};
		context.ClearRenderTarget(renderTargetView, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		if (depthStencilView != nullptr)
			context.ClearDepthStencil(depthStencilView, CLEAR_DEPTH_FLAG, 1.0f, 0,
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
		if (!m_shared->immediateContext || !m_swapChain) return;

		const auto& swapChainDesc = m_swapChain->GetDesc();
		Viewport viewport{};
		viewport.Width = static_cast<float>(swapChainDesc.Width);
		viewport.Height = static_cast<float>(swapChainDesc.Height);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		m_shared->immediateContext->SetViewports(1, &viewport, swapChainDesc.Width, swapChainDesc.Height);

		for (const auto& item : m_renderQueue)
		{
			if (const UploadedMesh* mesh = FindUploaded(m_shared->uploadedMeshes, item.meshId.value); mesh != nullptr)
				DrawMesh(item, *mesh);
			else
				DrawPrimitive(item);
		}
	}

	void DiligentDX12Adapter::DrawMesh(const RenderItem& item, const UploadedMesh& mesh)
	{
		IPipelineState* pipelineState = m_shared->meshPipeline.pipelineState;
		IShaderResourceBinding* shaderResourceBinding = m_shared->meshPipeline.shaderResourceBinding;

		if (UploadedShaderProgram* shaderProgram = FindUploaded(m_shared->uploadedShaderPrograms, item.shaderId.value);
		    shaderProgram != nullptr)
		{
			const auto* texture = FindUploaded(m_shared->uploadedTextures, item.textureId.value);
			IShaderResourceBinding* textureBinding = GetShaderResourceBinding(*shaderProgram, item.textureId);
			if (texture != nullptr && textureBinding != nullptr)
			{
				pipelineState =
				    shaderProgram->pipelineStates[GetSamplerVariantIndex(texture->filterMode, texture->addressMode)];
				shaderResourceBinding = textureBinding;
			}
			else
			{
				NENE_LOG_WARN("DiligentDX12Adapter: shader {} requires texture {}, falling back to default mesh "
				              "pipeline for mesh {}",
				              item.shaderId.value, item.textureId.value, item.meshId.value);
			}
		}

		if (pipelineState == nullptr || !m_shared->meshPipeline.constantBuffer)
		{
			NENE_LOG_WARN("DiligentDX12Adapter: mesh pipeline resources are missing for mesh {}", item.meshId.value);
			return;
		}

		if (!WriteDrawConstants(m_shared->meshPipeline.constantBuffer, item)) return;

		IDeviceContext& context = *m_shared->immediateContext;
		IBuffer* vertexBuffers[] = {mesh.vertexBuffer};
		const Uint64 offsets[] = {0};

		context.SetPipelineState(pipelineState);
		context.SetVertexBuffers(0, 1, vertexBuffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
		                         SET_VERTEX_BUFFERS_FLAG_RESET);
		context.SetIndexBuffer(mesh.indexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		if (shaderResourceBinding != nullptr)
			context.CommitShaderResources(shaderResourceBinding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		context.DrawIndexed(DrawIndexedAttribs{mesh.indexCount, VT_UINT32, kDrawVerifyFlags});

		NENE_LOG_DEBUG("DiligentDX12Adapter: drew uploaded mesh={} material={} shader={} indices={} "
		               "texture={} tint=({:.2f}, {:.2f}, {:.2f}, {:.2f})",
		               item.meshId.value, item.materialId.value, item.shaderId.value, mesh.indexCount,
		               item.textureId.value, item.tint.r, item.tint.g, item.tint.b, item.tint.a);
	}

	void DiligentDX12Adapter::DrawPrimitive(const RenderItem& item)
	{
		const DrawPipeline& pipeline = m_shared->primitivePipelines[static_cast<size_t>(item.primitiveType)];
		if (!pipeline.pipelineState || !pipeline.constantBuffer)
		{
			NENE_LOG_WARN("DiligentDX12Adapter: resources are missing for primitive type {}",
			              static_cast<int>(item.primitiveType));
			return;
		}

		if (!WriteDrawConstants(pipeline.constantBuffer, item)) return;

		IDeviceContext& context = *m_shared->immediateContext;
		context.SetPipelineState(pipeline.pipelineState);
		if (pipeline.shaderResourceBinding)
			context.CommitShaderResources(pipeline.shaderResourceBinding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		DrawAttribs drawAttribs{};
		drawAttribs.NumVertices = GetPrimitiveVertexCount(item.primitiveType);
		drawAttribs.Flags = kDrawVerifyFlags;
		context.Draw(drawAttribs);

		NENE_LOG_DEBUG("DiligentDX12Adapter: drew primitive={} mesh={} material={} shader={} tint=({:.2f}, "
		               "{:.2f}, {:.2f}, {:.2f})",
		               static_cast<int>(item.primitiveType), item.meshId.value, item.materialId.value,
		               item.shaderId.value, item.tint.r, item.tint.g, item.tint.b, item.tint.a);
	}

	bool DiligentDX12Adapter::WriteDrawConstants(IBuffer* constantBuffer, const RenderItem& item)
	{
		IDeviceContext& context = *m_shared->immediateContext;

		PVoid mappedData = nullptr;
		context.MapBuffer(constantBuffer, MAP_WRITE, MAP_FLAG_DISCARD, mappedData);
		if (mappedData == nullptr)
		{
			NENE_LOG_WARN("DiligentDX12Adapter: failed to map constant buffer");
			return false;
		}

		auto* drawConstants = static_cast<DrawConstants*>(mappedData);
		drawConstants->modelViewProjectionMatrix = item.modelViewProjectionMatrix;
		drawConstants->tint = item.tint;
		context.UnmapBuffer(constantBuffer, MAP_WRITE);
		return true;
	}

	void DiligentDX12Adapter::Present(uint32_t syncInterval)
	{
		if (m_swapChain) m_swapChain->Present(syncInterval);
	}

	void DiligentDX12Adapter::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;

		if (m_shared->immediateContext) m_shared->immediateContext->Flush();

		if (m_swapChain)
		{
			m_swapChain->Resize(width, height);
			NENE_LOG_INFO("DiligentDX12Adapter: swap chain resized to {}x{}", width, height);
		}
	}

	void DiligentDX12Adapter::SetClearColor(const glm::vec4& color)
	{
		m_clearColor = color;
		NENE_LOG_INFO("DiligentDX12Adapter: clear color set to ({:.2f}, {:.2f}, {:.2f}, {:.2f})", m_clearColor.r,
		              m_clearColor.g, m_clearColor.b, m_clearColor.a);
	}

	IShaderResourceBinding* DiligentDX12Adapter::GetShaderResourceBinding(UploadedShaderProgram& shaderProgram,
	                                                                      TextureId textureId)
	{
		if (!textureId.IsValid()) return nullptr;

		if (const auto cached = shaderProgram.srbsByTexture.find(textureId.value);
		    cached != shaderProgram.srbsByTexture.end())
			return cached->second;

		const auto* texture = FindUploaded(m_shared->uploadedTextures, textureId.value);
		if (texture == nullptr || !texture->shaderResourceView) return nullptr;

		IPipelineState* pipelineState =
		    shaderProgram.pipelineStates[GetSamplerVariantIndex(texture->filterMode, texture->addressMode)];
		if (pipelineState == nullptr) return nullptr;

		// SRBs are cached per texture because the mutable texture binding differs for each material.
		RefCntAutoPtr<IShaderResourceBinding> srb;
		pipelineState->CreateShaderResourceBinding(&srb, true);
		if (!srb) return nullptr;

		if (auto* textureVariable = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture"); textureVariable != nullptr)
			textureVariable->Set(texture->shaderResourceView);

		auto [inserted, _] = shaderProgram.srbsByTexture.emplace(textureId.value, std::move(srb));
		return inserted->second;
	}

} // namespace NeneEngine

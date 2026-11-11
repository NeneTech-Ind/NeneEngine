// DiligentDX12Adapter.cpp

#include "Graphics/Backend/DiligentDX12Adapter.h"

#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsAccessories/interface/GraphicsAccessories.hpp"
#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/Shader.h"
#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "../external/DiligentEngine/DiligentTools/TextureLoader/interface/TextureLoader.h"

#include "Core/NeneLogger.h"
#include "Core/Profiler.h"

#include <memory>
#include <stdexcept>
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
		constexpr Uint8 kGraphicsQueueId = 0;
		constexpr Uint8 kTransferQueueId = 2;
		constexpr Uint64 kGraphicsAndTransferContexts = 0b11;

		struct DrawConstants
		{
			glm::mat4 modelViewProjectionMatrix = glm::mat4(1.0f);
			glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
		};

		TEXTURE_FORMAT ColorStorageFormat(TEXTURE_FORMAT format)
		{
			// Typeless storage allows a linear-sampling scene SRV and an encoded-color UI SRV.
			switch (format)
			{
			case TEX_FORMAT_RGBA8_UNORM_SRGB:
				return TEX_FORMAT_RGBA8_TYPELESS;
			case TEX_FORMAT_BGRA8_UNORM_SRGB:
				return TEX_FORMAT_BGRA8_TYPELESS;
			case TEX_FORMAT_BGRX8_UNORM_SRGB:
				return TEX_FORMAT_BGRX8_TYPELESS;
			case TEX_FORMAT_BC1_UNORM_SRGB:
				return TEX_FORMAT_BC1_TYPELESS;
			case TEX_FORMAT_BC2_UNORM_SRGB:
				return TEX_FORMAT_BC2_TYPELESS;
			case TEX_FORMAT_BC3_UNORM_SRGB:
				return TEX_FORMAT_BC3_TYPELESS;
			case TEX_FORMAT_BC7_UNORM_SRGB:
				return TEX_FORMAT_BC7_TYPELESS;
			default:
				return format;
			}
		}

		RefCntAutoPtr<ITextureView> CreateColorView(ITexture& texture, TEXTURE_VIEW_TYPE type, TEXTURE_FORMAT format)
		{
			TextureViewDesc desc{};
			desc.ViewType = type;
			desc.Format = format;
			RefCntAutoPtr<ITextureView> view;
			texture.CreateView(desc, &view);
			return view;
		}

		TEXTURE_FORMAT ImGuiSamplingFormat(TEXTURE_FORMAT textureFormat, TEXTURE_FORMAT framebufferFormat)
		{
			// Diligent's ImGui AUTO mode already decodes sRGB in its pixel shader.
			// Sample encoded bytes here so the same conversion is not also done by the SRV.
			return IsSRGBFormat(framebufferFormat) ? SRGBFormatToUnorm(textureFormat) : textureFormat;
		}

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

		RefCntAutoPtr<IBuffer> CreateBufferWithData(IRenderDevice& device, const char* name, BIND_FLAGS bindFlags,
		                                            const void* data, uint64_t sizeBytes,
		                                            IDeviceContext* context = nullptr,
		                                            Uint64 contextMask = BufferDesc{}.ImmediateContextMask)
		{
			BufferDesc bufferDesc{};
			bufferDesc.Name = name;
			bufferDesc.BindFlags = bindFlags;
			bufferDesc.Usage = USAGE_IMMUTABLE;
			bufferDesc.Size = sizeBytes;
			bufferDesc.ImmediateContextMask = contextMask;

			BufferData bufferData{};
			bufferData.pData = data;
			bufferData.DataSize = sizeBytes;
			bufferData.pContext = context;

			RefCntAutoPtr<IBuffer> buffer;
			device.CreateBuffer(bufferDesc, &bufferData, &buffer);
			return buffer;
		}

		class DiligentMeshUploadData final : public IMeshUploadData
		{
		  public:
			MeshData meshData;
			RefCntAutoPtr<IBuffer> vertexBuffer;
			RefCntAutoPtr<IBuffer> indexBuffer;
			uint32_t vertexCount = 0;
			uint32_t indexCount = 0;
		};

		template <typename Map> auto* FindUploaded(Map& resources, uint32_t id)
		{
			const auto it = id != 0 ? resources.find(id) : resources.end();
			return it != resources.end() ? std::addressof(it->second) : nullptr;
		}
	} // namespace

	DiligentDX12SharedDevice::~DiligentDX12SharedDevice()
	{
		if (transferContext) transferContext->Flush();
		if (immediateContext) immediateContext->Flush();
		if (device) device->IdleGPU();
	}

	bool DiligentDX12SharedDevice::InitializeDevice(IEngineFactoryD3D12& factory)
	{
		if (device && immediateContext) return true;

		EngineD3D12CreateInfo engineCreateInfo{};
		engineCreateInfo.EnableValidation = kEnableGpuValidation;

		const ImmediateContextCreateInfo contextInfos[] = {{"Graphics", kGraphicsQueueId},
		                                                   {"Transfer", kTransferQueueId}};
		engineCreateInfo.NumImmediateContexts = _countof(contextInfos);
		engineCreateInfo.pImmediateContextInfo = contextInfos;
		IDeviceContext* contexts[_countof(contextInfos)] = {};
		factory.CreateDeviceAndContextsD3D12(engineCreateInfo, &device, contexts);
		immediateContext.Attach(contexts[0]);
		transferContext.Attach(contexts[1]);

		if (!device || !immediateContext)
		{
			NENE_LOG_WARN("DiligentDX12Adapter: could not create a transfer queue, using the graphics queue only");
			transferContext.Release();
			immediateContext.Release();
			device.Release();
			engineCreateInfo.NumImmediateContexts = 0;
			engineCreateInfo.pImmediateContextInfo = nullptr;
			factory.CreateDeviceAndContextsD3D12(engineCreateInfo, &device, &immediateContext);
		}

		if (!device || !immediateContext)
		{
			NENE_LOG_ERROR("Failed to create D3D12 device and immediate context");
			immediateContext.Release();
			device.Release();
			return false;
		}

		if (transferContext)
		{
			FenceDesc fenceDesc{};
			fenceDesc.Name = "Transfer Upload Fence";
			fenceDesc.Type = FENCE_TYPE_CPU_WAIT_ONLY;
			device->CreateFence(fenceDesc, &transferFence);
			if (!transferFence)
			{
				NENE_LOG_WARN("DiligentDX12Adapter: could not create the transfer fence, using the graphics queue only");
				transferContext.Release();
			}
		}

		NENE_LOG_INFO("DiligentDX12Adapter: created shared D3D12 device (validation={}, transfer queue={})",
		              kEnableGpuValidation, static_cast<bool>(transferContext));
		return true;
	}

	bool DiligentDX12SharedDevice::UsesTransferQueue() const
	{
		return device && transferContext && transferEnabled.load(std::memory_order_relaxed);
	}

	void DiligentDX12SharedDevice::RunOnTransferQueue(const eastl::function<void()>& record)
	{
		uint64_t value = 0;
		{
			std::scoped_lock lock(transferMutex);
			record();
			value = nextTransferFenceValue++;
			transferContext->EnqueueSignal(transferFence, value);
			transferContext->Flush();
			transferContext->FinishFrame();
		}

		NENE_PROFILE_SCOPE("Transfer Fence Wait");
		transferFence->Wait(value);
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
		m_activeTarget = {};
		m_renderTargets.clear();
		m_swapChain.Release();
	}

	uintptr_t DiligentDX12Adapter::GetResourceDomainKey() const
	{
		return reinterpret_cast<uintptr_t>(m_shared.get());
	}

	bool DiligentDX12Adapter::HasTransferQueue() const
	{
		return static_cast<bool>(m_shared->transferContext);
	}

	bool DiligentDX12Adapter::IsTransferQueueEnabled() const
	{
		return m_shared->UsesTransferQueue();
	}

	void DiligentDX12Adapter::SetTransferQueueEnabled(bool enabled)
	{
		m_shared->transferEnabled.store(enabled, std::memory_order_relaxed);
		NENE_LOG_INFO("DiligentDX12Adapter: transfer queue uploads {}", enabled ? "enabled" : "disabled");
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
		return RegisterBuffer(CreateBufferWithData(*m_shared->device, name, bindFlags, data, sizeBytes));
	}

	GPUBufferId DiligentDX12Adapter::RegisterBuffer(RefCntAutoPtr<IBuffer> buffer)
	{
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
		NENE_PROFILE_SCOPE("Mesh Upload");

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

		return RegisterMesh(vertexBuffer.bufferId, indexBuffer.bufferId, vertexCount, indexCount);
	}

	eastl::shared_ptr<IMeshUploadData> DiligentDX12Adapter::PrepareMesh(const MeshData& meshData) const
	{
		if (meshData.vertices.empty() || meshData.indices.empty())
			throw std::runtime_error("Mesh has no vertices or indices");

		auto prepared = eastl::make_shared<DiligentMeshUploadData>();
		prepared->vertexCount = static_cast<uint32_t>(meshData.vertices.size());
		prepared->indexCount = static_cast<uint32_t>(meshData.indices.size());
		if (!m_shared->UsesTransferQueue())
		{
			prepared->meshData = meshData;
			return prepared;
		}

		{
			NENE_PROFILE_SCOPE("Mesh Transfer Upload");
			m_shared->RunOnTransferQueue(
			    [&]()
			    {
				    prepared->vertexBuffer = CreateBufferWithData(
				        *m_shared->device, "Mesh Vertex Buffer", BIND_VERTEX_BUFFER, meshData.vertices.data(),
				        meshData.vertices.size() * sizeof(Vertex), m_shared->transferContext,
				        kGraphicsAndTransferContexts);
				    prepared->indexBuffer = CreateBufferWithData(
				        *m_shared->device, "Mesh Index Buffer", BIND_INDEX_BUFFER, meshData.indices.data(),
				        meshData.indices.size() * sizeof(uint32_t), m_shared->transferContext,
				        kGraphicsAndTransferContexts);
			    });
		}
		if (!prepared->vertexBuffer || !prepared->indexBuffer)
			throw std::runtime_error("Could not upload mesh buffers");
		prepared->vertexBuffer->SetState(RESOURCE_STATE_COMMON);
		prepared->indexBuffer->SetState(RESOURCE_STATE_COMMON);
		return prepared;
	}

	GPUMesh DiligentDX12Adapter::UploadMesh(const IMeshUploadData& data)
	{
		NENE_PROFILE_SCOPE("Mesh Create");
		if (!IsDeviceReady("UploadMesh")) return {};
		const auto* prepared = dynamic_cast<const DiligentMeshUploadData*>(&data);
		if (!prepared) return {};
		if (!prepared->vertexBuffer) return UploadMesh(prepared->meshData);

		const GPUBufferId vertexBufferId = RegisterBuffer(prepared->vertexBuffer);
		const GPUBufferId indexBufferId = RegisterBuffer(prepared->indexBuffer);
		return RegisterMesh(vertexBufferId, indexBufferId, prepared->vertexCount, prepared->indexCount);
	}

	GPUMesh DiligentDX12Adapter::RegisterMesh(GPUBufferId vertexBufferId, GPUBufferId indexBufferId,
	                                          uint32_t vertexCount, uint32_t indexCount)
	{
		UploadedMesh uploadedMesh{};
		uploadedMesh.vertexBuffer = *FindUploaded(m_shared->uploadedBuffers, vertexBufferId.value);
		uploadedMesh.indexBuffer = *FindUploaded(m_shared->uploadedBuffers, indexBufferId.value);
		uploadedMesh.indexCount = indexCount;

		const MeshId meshId{m_shared->nextMeshId++};
		m_shared->uploadedMeshes.emplace(meshId.value, std::move(uploadedMesh));

		NENE_LOG_INFO("DiligentDX12Adapter: uploaded mesh {} (vertices={}, indices={})", meshId.value, vertexCount,
		              indexCount);

		return GPUMesh{meshId, vertexBufferId, indexBufferId, vertexCount, indexCount};
	}

	namespace
	{
		class DiligentTextureUploadData final : public ITextureUploadData
		{
		  public:
			RefCntAutoPtr<ITextureLoader> loader;
			RefCntAutoPtr<ITexture> texture;
		};

		class DiligentShaderProgramUploadData final : public IShaderProgramUploadData
		{
		  public:
			DiligentDX12SharedDevice::UploadedShaderProgram program;
		};
	} // namespace

	eastl::shared_ptr<ITextureUploadData> DiligentDX12Adapter::PrepareTexture2D(const TextureResource& texture) const
	{
		NENE_PROFILE_SCOPE("Texture File Load");
		NENE_PROFILE_ZONE_TEXT(texture.path.data(), texture.path.size());
		TextureLoadInfo loadInfo{};
		loadInfo.IsSRGB = texture.isSrgb;
		auto prepared = eastl::make_shared<DiligentTextureUploadData>();
		CreateTextureLoaderFromFile(texture.path.c_str(), IMAGE_FILE_FORMAT_UNKNOWN, loadInfo, &prepared->loader);
		if (!prepared->loader) throw std::runtime_error("Could not decode texture: " + texture.path);
		if (!m_shared->UsesTransferQueue()) return prepared;

		auto storageDesc = prepared->loader->GetTextureDesc();
		storageDesc.Format = ColorStorageFormat(storageDesc.Format);
		storageDesc.ImmediateContextMask = kGraphicsAndTransferContexts;
		auto textureData = prepared->loader->GetTextureData();
		textureData.pContext = m_shared->transferContext;
		{
			NENE_PROFILE_SCOPE("Texture Transfer Upload");
			m_shared->RunOnTransferQueue(
			    [&]() { m_shared->device->CreateTexture(storageDesc, &textureData, &prepared->texture); });
		}
		if (!prepared->texture) throw std::runtime_error("Could not upload texture: " + texture.path);
		prepared->texture->SetState(RESOURCE_STATE_COMMON);
		return prepared;
	}

	GPUTexture DiligentDX12Adapter::CreateTexture2D(const TextureResource& texture)
	{
		const auto prepared = PrepareTexture2D(texture);
		return CreateTexture2D(texture, *prepared);
	}

	GPUTexture DiligentDX12Adapter::CreateTexture2D(const TextureResource& texture, const ITextureUploadData& data)
	{
		NENE_PROFILE_SCOPE("Texture Create");
		NENE_PROFILE_ZONE_TEXT(texture.path.data(), texture.path.size());
		if (!IsDeviceReady("CreateTexture2D")) return {};
		const auto* prepared = dynamic_cast<const DiligentTextureUploadData*>(&data);
		if (!prepared || !prepared->loader) return {};
		const auto& textureLoader = prepared->loader;

		DiligentDX12SharedDevice::UploadedTexture uploadedTexture{};
		const auto sourceFormat = textureLoader->GetTextureDesc().Format;
		uploadedTexture.texture = prepared->texture;
		if (!uploadedTexture.texture)
		{
			NENE_PROFILE_SCOPE("Texture Upload");
			auto storageDesc = textureLoader->GetTextureDesc();
			storageDesc.Format = ColorStorageFormat(sourceFormat);
			const auto textureData = textureLoader->GetTextureData();
			m_shared->device->CreateTexture(storageDesc, &textureData, &uploadedTexture.texture);
		}
		if (!uploadedTexture.texture)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: failed to create GPU texture for '{}'", texture.path);
			return {};
		}

		uploadedTexture.filterMode = texture.filterMode;
		uploadedTexture.addressMode = texture.addressMode;
		uploadedTexture.shaderResourceView =
		    CreateColorView(*uploadedTexture.texture, TEXTURE_VIEW_SHADER_RESOURCE, sourceFormat);
		uploadedTexture.imguiView = CreateColorView(*uploadedTexture.texture, TEXTURE_VIEW_SHADER_RESOURCE,
		                                            ImGuiSamplingFormat(sourceFormat, m_shared->colorBufferFormat));
		if (!uploadedTexture.shaderResourceView || !uploadedTexture.imguiView)
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

	GPUTexture DiligentDX12Adapter::CreatePlaceholderTexture()
	{
		if (!IsDeviceReady("CreatePlaceholderTexture")) return {};
		constexpr uint32_t kPixels[] = {0xFF555555, 0xFFBBBBBB, 0xFFBBBBBB, 0xFF555555};
		TextureDesc desc{};
		desc.Name = "Loading placeholder";
		desc.Type = RESOURCE_DIM_TEX_2D;
		desc.Width = desc.Height = 2;
		desc.Format = TEX_FORMAT_RGBA8_UNORM;
		desc.Usage = USAGE_IMMUTABLE;
		desc.BindFlags = BIND_SHADER_RESOURCE;
		TextureSubResData subresource{};
		subresource.pData = kPixels;
		subresource.Stride = 2 * sizeof(uint32_t);
		TextureData initialData{&subresource, 1};
		DiligentDX12SharedDevice::UploadedTexture uploaded{};
		m_shared->device->CreateTexture(desc, &initialData, &uploaded.texture);
		if (!uploaded.texture) return {};
		uploaded.shaderResourceView = uploaded.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
		uploaded.imguiView = uploaded.shaderResourceView;
		const TextureId id{m_shared->nextTextureId++};
		m_shared->uploadedTextures.emplace(id.value, std::move(uploaded));
		return {id, 2, 2};
	}

	eastl::shared_ptr<IShaderProgramUploadData> DiligentDX12Adapter::PrepareShaderProgram(
	    const ShaderProgramResource& shaderProgram) const
	{
		NENE_PROFILE_SCOPE("Shader Program Compile");
		NENE_PROFILE_ZONE_TEXT(shaderProgram.vertexPath.data(), shaderProgram.vertexPath.size());
		NENE_PROFILE_ZONE_TEXT(shaderProgram.pixelPath.data(), shaderProgram.pixelPath.size());

		if (!m_shared->device || !m_shared->pipelinesCreated)
			throw std::runtime_error("Shader program requested before renderer initialization");

		const ShaderPair shaders =
		    CreateShaderPair(*m_shared->device, shaderProgram.vertexPath.c_str(), shaderProgram.vertexSource.c_str(),
		                     shaderProgram.pixelPath.c_str(), shaderProgram.pixelSource.c_str());
		if (!shaders.IsValid())
			throw std::runtime_error("Could not compile shader program: " + shaderProgram.vertexPath + ", " +
			                         shaderProgram.pixelPath);

		GraphicsPipelineStateCreateInfo createInfo{};
		ConfigureGraphicsPipeline(createInfo, "Resource Shader Program PSO", m_shared->colorBufferFormat,
		                          m_shared->depthBufferFormat, PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true);
		SetMeshInputLayout(createInfo);
		createInfo.PSODesc.ResourceLayout.Variables = kTexturedMeshVariables;
		createInfo.PSODesc.ResourceLayout.NumVariables = _countof(kTexturedMeshVariables);
		createInfo.pVS = shaders.vertexShader;
		createInfo.pPS = shaders.pixelShader;

		auto prepared = eastl::make_shared<DiligentShaderProgramUploadData>();
		for (const TextureAddressMode addressMode : {TextureAddressMode::Wrap, TextureAddressMode::Clamp})
		{
			for (const TextureFilterMode filterMode : {TextureFilterMode::Linear, TextureFilterMode::Nearest})
			{
				const ImmutableSamplerDesc immutableSamplers[] = {
				    {SHADER_TYPE_PIXEL, "g_Texture", MakeSamplerDesc(filterMode, addressMode)}};
				createInfo.PSODesc.ResourceLayout.ImmutableSamplers = immutableSamplers;
				createInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(immutableSamplers);

				auto& pipelineState = prepared->program.pipelineStates[GetSamplerVariantIndex(filterMode, addressMode)];
				m_shared->device->CreateGraphicsPipelineState(createInfo, &pipelineState);
				if (!pipelineState) throw std::runtime_error("Could not create shader program pipelines");
			}
		}
		return prepared;
	}

	GPUShaderProgram DiligentDX12Adapter::CreateShaderProgram(const ShaderProgramResource& shaderProgram)
	{
		try
		{
			const auto prepared = PrepareShaderProgram(shaderProgram);
			return CreateShaderProgram(shaderProgram, *prepared);
		}
		catch (const std::exception& exception)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: {}", exception.what());
			return {};
		}
	}

	GPUShaderProgram DiligentDX12Adapter::CreateShaderProgram(const ShaderProgramResource& shaderProgram,
	                                                          const IShaderProgramUploadData& data)
	{
		NENE_PROFILE_SCOPE("Shader Program Create");
		NENE_PROFILE_ZONE_TEXT(shaderProgram.vertexPath.data(), shaderProgram.vertexPath.size());

		if (!m_shared->device || !m_shared->pipelinesCreated)
		{
			NENE_LOG_ERROR("DiligentDX12Adapter: CreateShaderProgram called before renderer initialization");
			return {};
		}
		const auto* prepared = dynamic_cast<const DiligentShaderProgramUploadData*>(&data);
		if (!prepared) return {};

		if (!m_shared->whiteTextureId.IsValid())
		{
			TextureDesc desc{};
			desc.Name = "Material white texture";
			desc.Type = RESOURCE_DIM_TEX_2D;
			desc.Width = desc.Height = 1;
			desc.Format = TEX_FORMAT_RGBA8_UNORM;
			desc.BindFlags = BIND_SHADER_RESOURCE;
			desc.Usage = USAGE_IMMUTABLE;
			const uint32_t white = 0xffffffffu;
			TextureSubResData subresource{};
			subresource.pData = &white;
			subresource.Stride = sizeof(white);
			TextureData data{};
			data.pSubResources = &subresource;
			data.NumSubresources = 1;
			DiligentDX12SharedDevice::UploadedTexture texture;
			m_shared->device->CreateTexture(desc, &data, &texture.texture);
			if (!texture.texture) return {};
			texture.shaderResourceView = texture.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
			texture.imguiView = texture.shaderResourceView;
			m_shared->whiteTextureId = TextureId{m_shared->nextTextureId++};
			m_shared->uploadedTextures.emplace(m_shared->whiteTextureId.value, std::move(texture));
		}

		UploadedShaderProgram uploadedShader{};
		uploadedShader.pipelineStates = prepared->program.pipelineStates;
		for (auto& pipelineState : uploadedShader.pipelineStates)
		{
			if (!pipelineState) return {};
			if (auto* constantsVariable = pipelineState->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants"))
				constantsVariable->Set(m_shared->meshPipeline.constantBuffer);
		}

		const ShaderId shaderId{m_shared->nextShaderId++};
		m_shared->uploadedShaderPrograms.emplace(shaderId.value, std::move(uploadedShader));
		NENE_LOG_INFO("DiligentDX12Adapter: created shader program {}", shaderId.value);
		return GPUShaderProgram{shaderId};
	}

	RenderTarget DiligentDX12Adapter::CreateRenderTarget(uint32_t width, uint32_t height)
	{
		if (!IsDeviceReady("CreateRenderTarget") || width == 0 || height == 0) return {};
		TextureDesc desc{};
		desc.Name = "Editor render target";
		desc.Type = RESOURCE_DIM_TEX_2D;
		desc.Width = width;
		desc.Height = height;
		desc.Format = ColorStorageFormat(m_shared->colorBufferFormat);
		desc.ClearValue.Format = m_shared->colorBufferFormat;
		desc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
		OffscreenTarget target;
		m_shared->device->CreateTexture(desc, nullptr, &target.color);
		if (!target.color) return {};
		target.renderTargetView =
		    CreateColorView(*target.color, TEXTURE_VIEW_RENDER_TARGET, m_shared->colorBufferFormat);
		target.imguiView =
		    CreateColorView(*target.color, TEXTURE_VIEW_SHADER_RESOURCE,
		                    ImGuiSamplingFormat(m_shared->colorBufferFormat, m_shared->colorBufferFormat));
		desc.Name = "Editor render target depth";
		desc.Format = m_shared->depthBufferFormat;
		desc.ClearValue.Format = m_shared->depthBufferFormat;
		desc.BindFlags = BIND_DEPTH_STENCIL;
		m_shared->device->CreateTexture(desc, nullptr, &target.depth);
		if (!target.renderTargetView || !target.imguiView || !target.depth) return {};
		const RenderTargetId id{m_nextTargetId++};
		m_renderTargets.emplace(id.value, std::move(target));
		return {id, width, height};
	}

	void DiligentDX12Adapter::DestroyRenderTarget(RenderTargetId target)
	{
		if (m_activeTarget == target) m_activeTarget = {};
		m_renderTargets.erase(target.value);
	}

	void DiligentDX12Adapter::SetRenderTarget(RenderTargetId target)
	{
		m_activeTarget = m_renderTargets.find(target.value) != m_renderTargets.end() ? target : RenderTargetId{};
	}

	uintptr_t DiligentDX12Adapter::GetRenderTargetTextureHandle(RenderTargetId target) const
	{
		const auto it = m_renderTargets.find(target.value);
		return it == m_renderTargets.end() ? 0 : reinterpret_cast<uintptr_t>(it->second.imguiView.RawPtr());
	}

	uintptr_t DiligentDX12Adapter::GetTextureHandle(TextureId texture) const
	{
		const auto it = m_shared->uploadedTextures.find(texture.value);
		return it == m_shared->uploadedTextures.end() ? 0 : reinterpret_cast<uintptr_t>(it->second.imguiView.RawPtr());
	}

	void DiligentDX12Adapter::BeginFrame()
	{
		if (!m_swapChain || !m_shared->immediateContext) return;

		ITextureView* renderTargetView = m_swapChain->GetCurrentBackBufferRTV();
		ITextureView* depthStencilView = m_swapChain->GetDepthBufferDSV();
		if (const auto it = m_renderTargets.find(m_activeTarget.value); it != m_renderTargets.end())
		{
			renderTargetView = it->second.renderTargetView;
			depthStencilView = it->second.depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
		}
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
		const auto targetIt = m_renderTargets.find(m_activeTarget.value);
		const uint32_t width =
		    targetIt != m_renderTargets.end() ? targetIt->second.color->GetDesc().Width : swapChainDesc.Width;
		const uint32_t height =
		    targetIt != m_renderTargets.end() ? targetIt->second.color->GetDesc().Height : swapChainDesc.Height;
		Viewport viewport{};
		viewport.Width = static_cast<float>(width);
		viewport.Height = static_cast<float>(height);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		m_shared->immediateContext->SetViewports(1, &viewport, width, height);

		for (const auto& item : m_renderQueue)
		{
			if (const UploadedMesh* mesh = FindUploaded(m_shared->uploadedMeshes, item.meshId.value); mesh != nullptr)
				DrawMesh(item, *mesh);
			else
				DrawPrimitive(item);
		}

		if (targetIt != m_renderTargets.end())
		{
			// Unbind before sampling the scene in ImGui, then restore the full-window UI target.
			ITextureView* backBuffer = m_swapChain->GetCurrentBackBufferRTV();
			m_shared->immediateContext->SetRenderTargets(1, &backBuffer, m_swapChain->GetDepthBufferDSV(),
			                                             RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			StateTransitionDesc barrier(targetIt->second.color, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE,
			                            STATE_TRANSITION_FLAG_UPDATE_STATE);
			m_shared->immediateContext->TransitionResourceState(barrier);
			const float background[] = {0.08f, 0.08f, 0.08f, 1.0f};
			m_shared->immediateContext->ClearRenderTarget(backBuffer, background,
			                                              RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		}
	}

	void DiligentDX12Adapter::DrawMesh(const RenderItem& item, const UploadedMesh& mesh)
	{
		IPipelineState* pipelineState = m_shared->meshPipeline.pipelineState;
		IShaderResourceBinding* shaderResourceBinding = m_shared->meshPipeline.shaderResourceBinding;

		if (UploadedShaderProgram* shaderProgram = FindUploaded(m_shared->uploadedShaderPrograms, item.shaderId.value);
		    shaderProgram != nullptr)
		{
			const TextureId textureId = item.textureId.IsValid() ? item.textureId : m_shared->whiteTextureId;
			const auto* texture = FindUploaded(m_shared->uploadedTextures, textureId.value);
			IShaderResourceBinding* textureBinding = GetShaderResourceBinding(*shaderProgram, textureId);
			if (texture != nullptr && textureBinding != nullptr)
			{
				pipelineState =
				    shaderProgram->pipelineStates[GetSamplerVariantIndex(texture->filterMode, texture->addressMode)];
				shaderResourceBinding = textureBinding;
			}
			else
			{
				const uint64_t fallbackKey = (static_cast<uint64_t>(item.shaderId.value) << 32) | item.meshId.value;
				if (m_reportedTextureFallbacks.insert(fallbackKey).second)
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

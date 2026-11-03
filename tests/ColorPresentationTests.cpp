#include "Graphics/Backend/DiligentDX12Adapter.h"
#include "Graphics/Backend/DiligentImGuiBackend.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <imgui.h>

using namespace NeneEngine;
using namespace Diligent;

namespace
{
	using Pixel = std::array<unsigned char, 4>;

	Pixel ReadCenter(DiligentDX12Adapter& renderer)
	{
		auto* source = renderer.GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture();
		auto desc = source->GetDesc();
		desc.Name = "Color presentation readback";
		desc.Usage = USAGE_STAGING;
		desc.BindFlags = BIND_NONE;
		desc.CPUAccessFlags = CPU_ACCESS_READ;
		RefCntAutoPtr<ITexture> staging;
		renderer.GetDevice()->CreateTexture(desc, nullptr, &staging);
		if (!staging) throw std::runtime_error("Could not create color readback texture");
		auto* context = renderer.GetImmediateContext();
		CopyTextureAttribs copy;
		copy.pSrcTexture = source;
		copy.pDstTexture = staging;
		copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
		copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
		context->CopyTexture(copy);
		context->WaitForIdle();
		MappedTextureSubresource mapped;
		context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, mapped);
		if (!mapped.pData) throw std::runtime_error("Could not read color presentation pixel");
		const auto* pixel =
		    static_cast<const unsigned char*>(mapped.pData) + (desc.Height / 2) * mapped.Stride + (desc.Width / 2) * 4;
		const Pixel result{pixel[0], pixel[1], pixel[2], pixel[3]};
		context->UnmapTextureSubresource(staging, 0, 0);
		return result;
	}

	void CheckColor(const Pixel& actual, const Pixel& expected, const char* label)
	{
		std::cout << label << ": " << int(actual[0]) << ',' << int(actual[1]) << ',' << int(actual[2]) << '\n';
		for (size_t channel = 0; channel < 4; ++channel)
			if (std::abs(int(actual[channel]) - int(expected[channel])) > 1) throw std::runtime_error(label);
	}

	void ImageFrame(DiligentImGuiBackend& ui, uintptr_t texture)
	{
		ui.BeginFrame();
		ImGui::SetNextWindowPos({0, 0});
		ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("Color probe", nullptr,
		             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
		                 ImGuiWindowFlags_NoBackground);
		ImGui::Image(static_cast<ImTextureID>(texture), ImGui::GetContentRegionAvail());
		ImGui::End();
		ImGui::PopStyleVar();
	}
} // namespace

int main()
{
	WNDCLASSW windowClass{};
	windowClass.lpfnWndProc = DefWindowProcW;
	windowClass.hInstance = GetModuleHandleW(nullptr);
	windowClass.lpszClassName = L"NeneColorPresentationTest";
	RegisterClassW(&windowClass);
	const HWND window = CreateWindowW(windowClass.lpszClassName, L"Color presentation test", WS_POPUP, 0, 0, 256, 256,
	                                  nullptr, nullptr, windowClass.hInstance, nullptr);
	if (!window) return 77;
	int result = 0;
	try
	{
		auto sharedDevice = eastl::make_shared<DiligentDX12SharedDevice>();
		DiligentDX12Adapter renderer(sharedDevice);
		if (!renderer.Init(window, 256, 256))
		{
			DestroyWindow(window);
			return 77;
		}
		{
			DiligentImGuiBackend ui(renderer, window, {});
			ImGui::GetIO().IniFilename = nullptr;
			renderer.SetClearColor({0.25f, 0.5f, 0.75f, 1});
			renderer.BeginFrame();
			renderer.EndFrame();
			const auto direct = ReadCenter(renderer);
			const auto target = renderer.CreateRenderTarget(256, 256);
			if (!target.IsValid()) throw std::runtime_error("Could not create viewport target");
			ImageFrame(ui, renderer.GetRenderTargetTextureHandle(target.id));
			renderer.SetRenderTarget(target.id);
			renderer.BeginFrame();
			renderer.EndFrame();
			ui.Render();
			CheckColor(ReadCenter(renderer), direct, "Viewport must preserve direct-render colors");

			// A 1x1 sRGB gray PNG must also appear as gray 128 in the texture inspector.
			constexpr unsigned char grayPng[] = {137, 80, 78,  71,  13,  10,  26, 10, 0,  0,   0,   13,  73,  72,
			                                     68,  82, 0,   0,   0,   1,   0,  0,  0,  1,   8,   6,   0,   0,
			                                     0,   31, 21,  196, 137, 0,   0,  0,  13, 73,  68,  65,  84,  120,
			                                     156, 99, 104, 104, 104, 248, 15, 0,  5,  132, 2,   128, 140, 205,
			                                     102, 38, 0,   0,   0,   0,   73, 69, 78, 68,  174, 66,  96,  130};
			std::filesystem::create_directories("out/qa");
			const auto path = std::filesystem::path("out/qa/color-probe.png");
			{
				std::ofstream file(path, std::ios::binary);
				file.write(reinterpret_cast<const char*>(grayPng), sizeof(grayPng));
			}
			const auto texture = renderer.CreateTexture2D(TextureResource{path.string()});
			if (!texture.IsValid()) throw std::runtime_error("Could not load gray probe texture");
			if (sharedDevice->uploadedTextures.at(texture.textureId.value).shaderResourceView->GetDesc().Format !=
			    TEX_FORMAT_RGBA8_UNORM_SRGB)
				throw std::runtime_error("Scene texture SRV must retain sRGB decoding");
			ImageFrame(ui, renderer.GetTextureHandle(texture.textureId));
			renderer.SetRenderTarget({});
			renderer.BeginFrame();
			renderer.EndFrame();
			ui.Render();
			CheckColor(ReadCenter(renderer), {128, 128, 128, 255}, "Texture preview must preserve source colors");
			renderer.DestroyRenderTarget(target.id);
		}
	}
	catch (const std::exception& error)
	{
		std::cerr << "FAIL " << error.what() << '\n';
		result = 1;
	}
	DestroyWindow(window);
	return result;
}

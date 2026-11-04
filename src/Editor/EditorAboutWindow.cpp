// EditorAboutWindow.cpp

#include "Editor/EditorAboutWindow.h"

#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"
#include "Editor/EditorContext.h"
#include "Editor/Theme/EditorIcons.h"
#include "Editor/Theme/EditorThemeConfig.h"
#include "Editor/Theme/EditorWidgets.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <string_view>

namespace NeneEngine
{
	namespace
	{
		constexpr const char* kWindowTitle = NENE_ICON_LOGO " About NeneEngine###AboutWindow";
		constexpr const char* kProductName = "NeneEngine";
		constexpr const char* kVersion = NENEENGINE_VERSION;
		constexpr const char* kBannerPath = "assets/readme/banner.png";
		constexpr const char* kNoticesPath = "THIRD_PARTY_NOTICES.txt";
		constexpr const char* kAuthors[] = {"BennySpace (Vlasov Daniil)", "Wesdmond (Timofey Shabanov)"};
		constexpr std::string_view kNoticesSectionEnd = "NOTES";
		constexpr std::string_view kLicenseField = "License:";
		constexpr std::string_view kCreditField = "Author / uploader credit:";
		constexpr std::string_view kCreditPrefix = "credit: ";
		constexpr float kWindowWidth = 560.0f;
		constexpr float kWindowHeight = 640.0f;

		std::string_view Trim(std::string_view text)
		{
			while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
			while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
			return text;
		}

		std::string_view ParseEntryName(std::string_view line)
		{
			size_t digits = 0;
			while (digits < line.size() && std::isdigit(static_cast<unsigned char>(line[digits]))) ++digits;
			if (digits == 0 || digits + 1 >= line.size() || line[digits] != '.') return {};
			return Trim(line.substr(digits + 1));
		}
	} // namespace

	void EditorAboutWindow::Initialize(IImGuiBackend& backend)
	{
		if (const std::filesystem::path bannerPath = ResolveFromExecutionRoots(kBannerPath); !bannerPath.empty())
			m_banner = backend.RegisterTexture(bannerPath);
		else
			NENE_LOG_WARN("About window: banner not found: {}", kBannerPath);

		m_notices.clear();
		const std::filesystem::path noticesPath = ResolveFromExecutionRoots(kNoticesPath);
		std::ifstream input(noticesPath);
		if (noticesPath.empty() || !input.is_open())
		{
			NENE_LOG_WARN("About window: {} not found", kNoticesPath);
			return;
		}

		std::string line;
		std::string_view pendingField;
		while (std::getline(input, line))
		{
			const std::string_view text = Trim(line);
			if (text == kNoticesSectionEnd) break;

			if (!pendingField.empty() && !text.empty())
			{
				if (!m_notices.empty() && m_notices.back().license.empty())
				{
					const bool isCredit = pendingField == kCreditField;
					const std::string prefix = isCredit ? std::string(kCreditPrefix) : std::string();
					m_notices.back().license = prefix + std::string(text);
				}
				pendingField = {};
				continue;
			}

			if (text == kLicenseField || text == kCreditField)
			{
				pendingField = text == kLicenseField ? kLicenseField : kCreditField;
				continue;
			}

			if (line.empty() || std::isspace(static_cast<unsigned char>(line.front()))) continue;
			if (const std::string_view name = ParseEntryName(text); !name.empty())
				m_notices.push_back({std::string(name), {}});
		}
	}

	void EditorAboutWindow::Shutdown(IImGuiBackend& backend)
	{
		if (m_banner.IsValid()) backend.ReleaseTexture(m_banner);
		m_banner = {};
		m_notices.clear();
		m_isOpen = false;
	}

	void EditorAboutWindow::Draw(const EditorContext& context)
	{
		if (!m_isOpen) return;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(ScaledSize(kWindowWidth), ScaledSize(kWindowHeight)), ImGuiCond_Appearing);
		constexpr ImGuiWindowFlags kFlags =
		    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
		if (ImGui::Begin(kWindowTitle, &m_isOpen, kFlags))
		{
			DrawBanner();

			if (context.themeColors != nullptr) ImGui::PushStyleColor(ImGuiCol_Text, context.themeColors->accent);
			ImGui::Text("%s %s", kProductName, kVersion);
			if (context.themeColors != nullptr) ImGui::PopStyleColor();

			ImGui::SeparatorText("Authors");
			for (const char* author : kAuthors) ImGui::BulletText("%s", author);

			ImGui::SeparatorText("Third-party components");
			DrawNotices();
		}
		ImGui::End();
	}

	void EditorAboutWindow::DrawBanner() const
	{
		if (!m_banner.IsValid() || m_banner.width == 0) return;

		const float width = ImGui::GetContentRegionAvail().x;
		const float height = width * static_cast<float>(m_banner.height) / static_cast<float>(m_banner.width);
		ImGui::Image(ImTextureRef(static_cast<ImTextureID>(m_banner.id)), ImVec2(width, height));
	}

	void EditorAboutWindow::DrawNotices() const
	{
		if (m_notices.empty())
		{
			ImGui::TextDisabled("%s was not found", kNoticesPath);
			return;
		}

		constexpr ImGuiTableFlags kTableFlags =
		    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY;
		if (!ImGui::BeginTable("##Notices", 2, kTableFlags, ImVec2(0.0f, ImGui::GetContentRegionAvail().y))) return;

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Component");
		ImGui::TableSetupColumn("License");
		ImGui::TableHeadersRow();
		for (const NoticeEntry& notice : m_notices)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(notice.name.c_str());
			ImGui::TableNextColumn();
			ImGui::TextWrapped("%s", notice.license.c_str());
		}
		ImGui::EndTable();
	}

} // namespace NeneEngine

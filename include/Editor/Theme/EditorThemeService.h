// EditorThemeService.h

#pragma once

#include "Editor/Theme/EditorThemeConfig.h"

namespace NeneEngine
{
	class EditorThemeService final
	{
	  public:
		void Apply(const EditorThemeConfig& config);
		void LoadFonts();
		void SetDpiScale(float scale);

		[[nodiscard]] EditorThemeVariant GetVariant() const { return m_config.variant; }
		[[nodiscard]] float GetDpiScale() const { return m_dpiScale; }
		[[nodiscard]] const EditorThemeColorsConfig& GetColors() const { return m_config.colors; }
		[[nodiscard]] const EditorThemeMetricsConfig& GetMetrics() const { return m_config.metrics; }
		[[nodiscard]] ImFont* GetMonospaceFont() const { return m_monospaceFont; }

	  private:
		void ApplyColors() const;
		void ApplyMetrics() const;

		EditorThemeConfig m_config{};
		ImFont* m_uiFont = nullptr;
		ImFont* m_monospaceFont = nullptr;
		float m_dpiScale = 1.0f;
	};

} // namespace NeneEngine

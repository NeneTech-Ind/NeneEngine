// EditorThemeService.h

#pragma once

#include "Editor/Theme/EditorThemeConfig.h"

namespace NeneEngine
{
	class EditorThemeService final
	{
	  public:
		void Apply(const EditorThemeConfig& config);

		[[nodiscard]] const EditorThemeColorsConfig& GetColors() const { return m_config.colors; }

	  private:
		void ApplyColors() const;
		void ApplyMetrics() const;

		EditorThemeConfig m_config{};
	};

} // namespace NeneEngine

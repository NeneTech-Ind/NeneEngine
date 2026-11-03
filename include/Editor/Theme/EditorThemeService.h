// EditorThemeService.h

#pragma once

#include "Editor/Theme/EditorThemeConfig.h"

namespace NeneEngine
{
	class EditorThemeService final
	{
	  public:
		void Apply(const EditorThemeConfig& config);

	  private:
		void ApplyColors() const;

		EditorThemeConfig m_config{};
	};

} // namespace NeneEngine

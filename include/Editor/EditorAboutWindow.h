// EditorAboutWindow.h

#pragma once

#include "Graphics/Backend/IImGuiBackend.h"

#include <EASTL/vector.h>
#include <string>

namespace NeneEngine
{
	struct EditorContext;

	class EditorAboutWindow final
	{
	  public:
		void Initialize(IImGuiBackend& backend);
		void Shutdown(IImGuiBackend& backend);

		void Open() { m_isOpen = true; }
		void Draw(const EditorContext& context);

	  private:
		struct NoticeEntry
		{
			std::string name;
			std::string license;
		};

		void DrawBanner() const;
		void DrawNotices() const;

		ImGuiTexture m_banner{};
		eastl::vector<NoticeEntry> m_notices;
		bool m_isOpen = false;
	};

} // namespace NeneEngine

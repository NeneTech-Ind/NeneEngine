// StatisticsPanel.h

#pragma once

#include "Editor/IEditorPanel.h"
#include "Editor/Theme/EditorIcons.h"

#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <cstddef>

namespace NeneEngine
{

	class StatisticsPanel final : public IEditorPanel
	{
	  public:
		[[nodiscard]] const char* GetName() const override { return "Statistics"; }
		[[nodiscard]] const char* GetIcon() const override { return NENE_ICON_STATISTICS; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Bottom; }

		void Update(EditorContext& context) override;
		void Draw(EditorContext& context) override;

	  private:
		static constexpr size_t kSampleCount = 600;

		void Reset();

		eastl::array<float, kSampleCount> m_frameTimesMs{};
		eastl::vector<float> m_sortedFrameTimesMs;
		size_t m_nextSample = 0;
		size_t m_sampleCount = 0;
	};

} // namespace NeneEngine

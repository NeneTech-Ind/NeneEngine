// PauseState.h

#pragma once

#include "IGameState.h"

namespace NeneEngine
{

	class PauseState final : public IGameState
	{
	  public:
		explicit PauseState(AppStateContext& context) : IGameState(context) {}

		void OnEnter() override;
		void OnPause() override;
		void OnResume() override;
		void OnExit() override;
		void Update(float dt) override;
		void HandleInput() override;

		bool IsTransparent() const override { return true; }
		bool IsPausing() const override { return false; }
	};

} // namespace NeneEngine

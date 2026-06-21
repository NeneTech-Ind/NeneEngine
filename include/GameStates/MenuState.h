// MenuState.h

#pragma once

#include "IGameState.h"

namespace NeneEngine
{

	class MenuState final : public IGameState
	{
	  public:
		explicit MenuState(AppStateContext& context) : IGameState(context) {}

		void OnEnter() override;
		void OnPause() override;
		void OnResume() override;
		void OnExit() override;
		void Update(float dt) override;
		void HandleInput() override;

		bool IsTransparent() const override { return false; }
		bool IsPausing() const override { return true; }
	};

} // namespace NeneEngine

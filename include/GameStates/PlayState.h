// PlayState.h

#pragma once

#include "IGameState.h"

namespace NeneEngine
{

	class PlayState final : public IGameState
	{
	  public:
		explicit PlayState(AppStateContext& context) : IGameState(context) {}

		void OnEnter() override;
		void OnPause() override;
		void OnResume() override;
		void OnExit() override;
		void Update(float dt) override;
		void HandleInput() override;

		bool IsTransparent() const override { return false; }
		bool IsPausing() const override { return false; }
	};

} // namespace NeneEngine

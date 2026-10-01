// InputActions.h

#pragma once

#include <string_view>

namespace NeneEngine::InputActions
{
	inline constexpr std::string_view Pause = "Pause";
	inline constexpr std::string_view Quit = "Quit";
	inline constexpr std::string_view MoveForward = "MoveForward";
	inline constexpr std::string_view MoveBackward = "MoveBackward";
	inline constexpr std::string_view MoveLeft = "MoveLeft";
	inline constexpr std::string_view MoveRight = "MoveRight";
	inline constexpr std::string_view MoveUp = "MoveUp";
	inline constexpr std::string_view MoveDown = "MoveDown";
	inline constexpr std::string_view Sprint = "Sprint";
	inline constexpr std::string_view LookModifier = "LookModifier";
	inline constexpr std::string_view PrimitiveMoveUp = "PrimitiveMoveUp";
	inline constexpr std::string_view PrimitiveMoveDown = "PrimitiveMoveDown";
	inline constexpr std::string_view PrimitiveMoveLeft = "PrimitiveMoveLeft";
	inline constexpr std::string_view PrimitiveMoveRight = "PrimitiveMoveRight";
	inline constexpr std::string_view ScaleStep = "ScaleStep";
	inline constexpr std::string_view RotateStep = "RotateStep";
	inline constexpr std::string_view ToggleDebugDraw = "ToggleDebugDraw";
} // namespace NeneEngine::InputActions

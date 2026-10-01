// KeyCodeStrings.cpp

#include "Input/KeyCodeStrings.h"

#include <EASTL/array.h>
#include <EASTL/utility.h>

namespace NeneEngine
{
	namespace
	{
		using KeyCodeNamePair = eastl::pair<std::string_view, KeyCode>;

		constexpr eastl::array<KeyCodeNamePair, 95> kKeyCodeNames{{
		    {"None", KeyCode::None},
		    {"MouseLeft", KeyCode::MouseLeft},
		    {"MouseRight", KeyCode::MouseRight},
		    {"MouseMiddle", KeyCode::MouseMiddle},
		    {"MouseX1", KeyCode::MouseX1},
		    {"MouseX2", KeyCode::MouseX2},
		    {"Backspace", KeyCode::Backspace},
		    {"Tab", KeyCode::Tab},
		    {"Enter", KeyCode::Enter},
		    {"Pause", KeyCode::Pause},
		    {"CapsLock", KeyCode::CapsLock},
		    {"Escape", KeyCode::Escape},
		    {"Space", KeyCode::Space},
		    {"PageUp", KeyCode::PageUp},
		    {"PageDown", KeyCode::PageDown},
		    {"End", KeyCode::End},
		    {"Home", KeyCode::Home},
		    {"Left", KeyCode::Left},
		    {"Up", KeyCode::Up},
		    {"Right", KeyCode::Right},
		    {"Down", KeyCode::Down},
		    {"PrintScreen", KeyCode::PrintScreen},
		    {"Insert", KeyCode::Insert},
		    {"Delete", KeyCode::Delete},
		    {"D0", KeyCode::D0},
		    {"D1", KeyCode::D1},
		    {"D2", KeyCode::D2},
		    {"D3", KeyCode::D3},
		    {"D4", KeyCode::D4},
		    {"D5", KeyCode::D5},
		    {"D6", KeyCode::D6},
		    {"D7", KeyCode::D7},
		    {"D8", KeyCode::D8},
		    {"D9", KeyCode::D9},
		    {"A", KeyCode::A},
		    {"B", KeyCode::B},
		    {"C", KeyCode::C},
		    {"D", KeyCode::D},
		    {"E", KeyCode::E},
		    {"F", KeyCode::F},
		    {"G", KeyCode::G},
		    {"H", KeyCode::H},
		    {"I", KeyCode::I},
		    {"J", KeyCode::J},
		    {"K", KeyCode::K},
		    {"L", KeyCode::L},
		    {"M", KeyCode::M},
		    {"N", KeyCode::N},
		    {"O", KeyCode::O},
		    {"P", KeyCode::P},
		    {"Q", KeyCode::Q},
		    {"R", KeyCode::R},
		    {"S", KeyCode::S},
		    {"T", KeyCode::T},
		    {"U", KeyCode::U},
		    {"V", KeyCode::V},
		    {"W", KeyCode::W},
		    {"X", KeyCode::X},
		    {"Y", KeyCode::Y},
		    {"Z", KeyCode::Z},
		    {"LeftWindows", KeyCode::LeftWindows},
		    {"RightWindows", KeyCode::RightWindows},
		    {"NumPad0", KeyCode::NumPad0},
		    {"NumPad1", KeyCode::NumPad1},
		    {"NumPad2", KeyCode::NumPad2},
		    {"NumPad3", KeyCode::NumPad3},
		    {"NumPad4", KeyCode::NumPad4},
		    {"NumPad5", KeyCode::NumPad5},
		    {"NumPad6", KeyCode::NumPad6},
		    {"NumPad7", KeyCode::NumPad7},
		    {"NumPad8", KeyCode::NumPad8},
		    {"NumPad9", KeyCode::NumPad9},
		    {"Multiply", KeyCode::Multiply},
		    {"Add", KeyCode::Add},
		    {"Subtract", KeyCode::Subtract},
		    {"Decimal", KeyCode::Decimal},
		    {"Divide", KeyCode::Divide},
		    {"F1", KeyCode::F1},
		    {"F2", KeyCode::F2},
		    {"F3", KeyCode::F3},
		    {"F4", KeyCode::F4},
		    {"F5", KeyCode::F5},
		    {"F6", KeyCode::F6},
		    {"F7", KeyCode::F7},
		    {"F8", KeyCode::F8},
		    {"F9", KeyCode::F9},
		    {"F10", KeyCode::F10},
		    {"F11", KeyCode::F11},
		    {"F12", KeyCode::F12},
		    {"LeftShift", KeyCode::LeftShift},
		    {"RightShift", KeyCode::RightShift},
		    {"LeftControl", KeyCode::LeftControl},
		    {"RightControl", KeyCode::RightControl},
		    {"LeftAlt", KeyCode::LeftAlt},
		    {"RightAlt", KeyCode::RightAlt},
		}};
	} // namespace

	std::string_view ToString(KeyCode keyCode)
	{
		for (const auto& [name, value] : kKeyCodeNames)
		{
			if (value == keyCode) return name;
		}

		return "Unknown";
	}

	eastl::optional<KeyCode> TryParseKeyCode(std::string_view value)
	{
		for (const auto& [name, keyCode] : kKeyCodeNames)
		{
			if (name == value) return keyCode;
		}

		return eastl::nullopt;
	}

} // namespace NeneEngine

#pragma once

// String-to-virtual-key mapping.
std::unordered_map<std::string, WORD> keyMap = {
	{"LMB", 0x1 }, {"RMB", 0x2 },
	// Alphabet keys.
	{"A", 'A'}, {"B", 'B'}, {"C", 'C'}, {"D", 'D'}, {"E", 'E'}, {"F", 'F'},
	{"G", 'G'}, {"H", 'H'}, {"I", 'I'}, {"J", 'J'}, {"K", 'K'}, {"L", 'L'},
	{"M", 'M'}, {"N", 'N'}, {"O", 'O'}, {"P", 'P'}, {"Q", 'Q'}, {"R", 'R'},
	{"S", 'S'}, {"T", 'T'}, {"U", 'U'}, {"V", 'V'}, {"W", 'W'}, {"X", 'X'},
	{"Y", 'Y'}, {"Z", 'Z'},

	// Digit keys.
	{"0", '0'}, {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'},
	{"5", '5'}, {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'},

	// Function keys.
	{"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4},
	{"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8},
	{"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},

	// Special keys.
	{"LSHIFT", VK_LSHIFT}, {"RSHIFT", VK_RSHIFT},
	{"LCTRL", VK_LCONTROL}, {"RCTRL", VK_RCONTROL},
	{"LALT", VK_LMENU}, {"RALT", VK_RMENU},
	{"SPACE", VK_SPACE}, {"ENTER", VK_RETURN},
	{"BACKSPACE", VK_BACK}, {"TAB", VK_TAB},
	{"CAPSLOCK", VK_CAPITAL}, {"ESCAPE", VK_ESCAPE},
	{"LEFT", VK_LEFT}, {"UP", VK_UP}, {"RIGHT", VK_RIGHT}, {"DOWN", VK_DOWN},
	{"PAGEUP", VK_PRIOR}, {"PAGEDOWN", VK_NEXT}, {"HOME", VK_HOME}, {"END", VK_END},
	{"INSERT", VK_INSERT}, {"DELETE", VK_DELETE},
	{"NUMLOCK", VK_NUMLOCK}, {"SCROLLLOCK", VK_SCROLL},

	// Numpad keys.
	{"NUMPAD0", VK_NUMPAD0}, {"NUMPAD1", VK_NUMPAD1}, {"NUMPAD2", VK_NUMPAD2},
	{"NUMPAD3", VK_NUMPAD3}, {"NUMPAD4", VK_NUMPAD4}, {"NUMPAD5", VK_NUMPAD5},
	{"NUMPAD6", VK_NUMPAD6}, {"NUMPAD7", VK_NUMPAD7}, {"NUMPAD8", VK_NUMPAD8},
	{"NUMPAD9", VK_NUMPAD9}, {"MULTIPLY", VK_MULTIPLY}, {"ADD", VK_ADD},
	{"SEPARATOR", VK_SEPARATOR}, {"SUBTRACT", VK_SUBTRACT}, {"DECIMAL", VK_DECIMAL},
	{"DIVIDE", VK_DIVIDE},

	// Extra symbols.
	{"SEMICOLON", VK_OEM_1}, {"PLUS", VK_OEM_PLUS}, {"COMMA", VK_OEM_COMMA},
	{"MINUS", VK_OEM_MINUS}, {"PERIOD", VK_OEM_PERIOD}, {"SLASH", VK_OEM_2},
	{"TILDE", VK_OEM_3}, {"LEFTBRACKET", VK_OEM_4}, {"BACKSLASH", VK_OEM_5},
	{"RIGHTBRACKET", VK_OEM_6}, {"QUOTE", VK_OEM_7}
};

void EmulateKeyPress(WORD vk_key, bool press, bool block_input = true)
{
	static HWND hwnd = FindWindowA(NULL, xorstr_("UKRAINE GTA"));
	if (hwnd == nullptr) hwnd = FindWindowA(NULL, xorstr_("MTA: San Andreas"));
	if (hwnd != nullptr)
	{
		/*if (callIsChatBoxInputEnabled != nullptr && CLocalGUI != nullptr)
		{
			if (callIsChatBoxInputEnabled(CLocalGUI) && block_input) return;
		}*/

		// Resolve scan code.
		UINT scan_code = MapVirtualKeyA(vk_key, MAPVK_VK_TO_VSC);
		bool is_extended = (vk_key == VK_RMENU || vk_key == VK_RCONTROL || vk_key == VK_NUMLOCK ||
			vk_key == VK_INSERT || vk_key == VK_DELETE || vk_key == VK_HOME ||
			vk_key == VK_END || vk_key == VK_PRIOR || vk_key == VK_NEXT ||
			vk_key == VK_UP || vk_key == VK_DOWN || vk_key == VK_LEFT ||
			vk_key == VK_RIGHT || vk_key == VK_DIVIDE || vk_key == VK_NUMPAD0 ||
			vk_key == VK_NUMPAD1 || vk_key == VK_NUMPAD2 || vk_key == VK_NUMPAD3 ||
			vk_key == VK_NUMPAD4 || vk_key == VK_NUMPAD5 || vk_key == VK_NUMPAD6 ||
			vk_key == VK_NUMPAD7 || vk_key == VK_NUMPAD8 || vk_key == VK_NUMPAD9);

		// Build lParam.
		LPARAM lParam = (1 & 0xFFFF); // Repeat count, usually 1 for emulation.
		lParam |= (scan_code << 16); // Scan code.
		if (is_extended) lParam |= (1 << 24); // Extended key flag.

		// Set flags for press/release.
		if (press)
		{
			lParam &= ~(1 << 30); // Previous state = 0 (key was up).
			lParam &= ~(1 << 31); // Transition state = 0 (key down).
		}
		else
		{
			lParam |= (1 << 30); // Previous state = 1 (key was down).
			lParam |= (1 << 31); // Transition state = 1 (key up).
		}

		// Send message.
		if (vk_key == VK_LMENU || vk_key == VK_RMENU) // Use WM_SYSKEY* for Alt keys.
		{
			if (press)
				PostMessageA(hwnd, WM_SYSKEYDOWN, vk_key, lParam);
			else
				PostMessageA(hwnd, WM_SYSKEYUP, vk_key, lParam);
		}
		else // Use WM_KEY* for all other keys.
		{
			if (press)
				PostMessageA(hwnd, WM_KEYDOWN, vk_key, lParam);
			else
				PostMessageA(hwnd, WM_KEYUP, vk_key, lParam);
		}
	}
}

#pragma once

namespace vendetta
{
	namespace input_bindings
	{
		enum class MOUSE_ACTION
		{
			MOUSE_MOVE_X,
			MOUSE_MOVE_Y,
			MOUSE_MIDDLE_PRESSED,
			MOUSE_MIDDLE_RELEASED,
			MOUSE_SCROLL_UP,
			MOUSE_SCROLL_DOWN,
			MOUSE_LEFT,
			MOUSE_RIGHT
		};

		enum class KEYBOARD_ACTION
		{
			KEYBOARD_FORWARD,
			KEYBOARD_RIGHT,
			KEYBOARD_SPACE,
			KEYBOARD_F,
			KEYBOARD_C
		};
	}
}
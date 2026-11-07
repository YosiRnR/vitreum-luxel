#pragma once

#include <cstdint>

namespace vitreum
{
	enum class WinEventType : uint32_t
	{
		None = 0,
		Close,
		Resize,
		KeyPress,
		KeyRepeat,
		KeyRelease,
		MouseMove,
		MouseButtonPress,
		MouseButtonRelease,
		MouseScroll,
		CharInput,
		FocusGained,
		FocusLost,
		DpiChanged			
	};

	enum class KeyCode : uint16_t
	{
	    Unknown = 0,
		// Letters and numbers...
        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        // Function keys...
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        // Modifiers...
        LeftShift, RightShift,
        LeftControl, RightControl,
        LeftAlt, RightAlt,
        LeftSuper, RightSuper, // Windows / Command
        // Navigation...
        LeftArrow, RightArrow, UpArrow, DownArrow,
        PageUp, PageDown, Home, End, Insert, Delete,
        // Others...
        Escape, Tab, CapsLock, Space, Enter, Backspace,
        PrintScreen, ScrollLock, Pause, NumLock,
        // KeyPad...
        KP_0, KP_1, KP_2, KP_3, KP_4, KP_5, KP_6, KP_7, KP_8, KP_9,
        KP_Decimal, KP_Divide, KP_Multiply, KP_Subtract, KP_Add, KP_Enter	
	};

	enum class MouseButton :  uint8_t { None = 0, Left, Middle, Right };
	enum class ActionState :  uint8_t { None = 0, Pressed, Released, Repeat };
	enum class KeyModifier : uint32_t
	{
		None = 0, Shift = 1 << 0, Control = 1 << 1, Alt = 1 << 2, Super = 1 << 3,
		CapsLock = 1 << 4, NumLock = 1 << 5, ScrollLock = 1 << 6
	};
	enum class MouseModifier : uint32_t
	{
		None = 0, Control = 1 << 1, LeftButton = 1 << 2, MiddleButton = 1 << 3, RigthButton = 1 << 4,
		Shift = 1 << 5, XButton1 = 1 << 6, XButton2 = 1 << 7
	};

	struct KeyEvent
	{
		KeyCode key{ KeyCode::Unknown };
		int scancode{ 0 };
		ActionState action{ ActionState::None };
		uint32_t modifiers{ 0 };
	};

	struct MouseButtonEvent
	{
		int xpos{ 0 };
		int ypos{ 0 };
		MouseButton button{ MouseButton::None };
		ActionState action{ ActionState::None };
		uint32_t modifiers{ 0 };
	};

	struct MouseMoveEvent
	{
		int xpos{ 0 };
		int ypos{ 0 };
	};

	struct MouseScrollEvent
	{
		float xoffset{ 0.f };
		float yoffset{ 0.f };
	};

	struct ResizeEvent
	{
		int width{ 0 };
		int height{ 0 };
	};

	struct TextEvent
	{
		char32_t codepoint{ 0 };
	};

	struct FocusEvent
	{
		int dpi_x{};
		int dpi_y{};
		Rect suggested_rect{};
	};

	struct DpiEvent
	{
		int dpi_x{};
		int dpi_y{};
		Rect suggested_rect{};
	};

	struct WinEvent
	{
		WinEventType type{ WinEventType::None };

		union
		{
			ResizeEvent size;
			KeyEvent key;
			MouseButtonEvent mouse_button;
			MouseMoveEvent mouse_move;
			MouseScrollEvent scroll;
			TextEvent text;
			FocusEvent focus;
			DpiEvent dpi;
		};
	};

} // nmespace vitreum


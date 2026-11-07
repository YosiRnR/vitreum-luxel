#include "types.h"
#include "window.h"
#include "winevent.h"
#include <span>
#include <memory>
#include <string>
#include <cstdint>
#include <string_view>

#if defined(_WIN32)

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#pragma comment(lib, "comdlg32.lib")

namespace vitreum
{
	static KeyCode TranslateVirtualKey(int virtual_key_code, LPARAM lparam);
	static uint32_t GetCurrentModifiers();
    static uint32_t GetCurrentMouseModifiers(WPARAM wparam);

	namespace
	{
		std::wstring utf8_to_wide(std::string_view value)
		{
			if (value.empty()) return {};

			const int length = MultiByteToWideChar(
					CP_UTF8,
					0,
					value.data(),
					static_cast<int>(value.size()),
					nullptr,
					0);

			if (length <= 0) return {};

			std::wstring result(static_cast<size_t>(length), L'\0');
            MultiByteToWideChar(
                CP_UTF8,
                0,
                value.data(),
                static_cast<int>(value.size()),
                result.data(),
                length);

            return result;
		}

	} // namespace
	
	struct Window::Impl
	{
		static LRESULT CALLBACK StaticWndProc(HWND, UINT, WPARAM, LPARAM);

		Impl() = default;
		~Impl() {
			if (hwnd) DestroyWindow(hwnd);
		}

		// Eliminar copia y movimiento
        Impl(const Impl&) = delete;
        Impl(Impl&&) = delete;
        Impl& operator=(const Impl&) = delete;
        Impl& operator=(Impl&&) = delete;

        WindowDescriptor window_desc;
        HINSTANCE        hinstance{ nullptr };
        WNDCLASSEX       wnd_classex{};
        HWND             hwnd{ nullptr };

        bool should_close{ false };

        std::vector<WinEvent> accumulated_events{};
        std::vector<WinEvent> frame_events{};
		uint32_t event_index{ 0 };

        Window* owner{ nullptr };

        CursorMode cursor_mode{ CursorMode::Normal };
        // Para restaurar el clip al salir de Disabled
        bool cursor_clipped{ false };
        RECT prev_clip_rect{};
        // Útil para el modo Disabled (recentrar)
        int last_cursor_x{ 0 };
        int last_cursor_y{ 0 };
	};

    Window::Window() : m_pimpl(std::make_unique<Impl>()) { m_pimpl->owner = this; }
    Window::~Window() = default;

    bool Window::create(const WindowDescriptor& wd)
    {
        m_pimpl->hinstance = GetModuleHandle(nullptr);
        m_pimpl->window_desc = wd;

        m_pimpl->wnd_classex.cbSize        = sizeof(WNDCLASSEX);
        m_pimpl->wnd_classex.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        m_pimpl->wnd_classex.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        m_pimpl->wnd_classex.hInstance     = m_pimpl->hinstance;
        m_pimpl->wnd_classex.lpfnWndProc   = Impl::StaticWndProc;
        m_pimpl->wnd_classex.lpszClassName = L"VitreumWindowClass";
        m_pimpl->wnd_classex.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

        if (!RegisterClassEx(&m_pimpl->wnd_classex)) {
            return false;
        }

        RECT target_rect{ 0, 0, wd.size.width, wd.size.height };
        AdjustWindowRect(&target_rect, WS_OVERLAPPEDWINDOW, false);

        m_pimpl->hwnd = CreateWindowEx(
            0,
            m_pimpl->wnd_classex.lpszClassName,
            std::wstring(wd.title.begin(), wd.title.end()).c_str(),
            WS_OVERLAPPEDWINDOW,
            wd.position.xpos, wd.position.ypos,
            target_rect.right - target_rect.left, target_rect.bottom - target_rect.top,
            nullptr,
            static_cast<HMENU>(nullptr),
            m_pimpl->hinstance,
            m_pimpl.get()
        );

        if (!m_pimpl->hwnd) return false;

        RECT client_rect{};
        GetClientRect(m_pimpl->hwnd, &client_rect);
        m_width  = client_rect.right  - client_rect.left;
        m_height = client_rect.bottom - client_rect.top;

        return true;
    }

    void Window::show() const {
        ShowWindow(m_pimpl->hwnd, SW_SHOW);
    }

    bool Window::should_close() const {
        return m_pimpl->should_close;
    }

    void Window::set_confirm_close(bool close) {
        m_pimpl->should_close = close;
    }

    float Window::content_scale() const
    {
        if (!m_pimpl || !m_pimpl->hwnd) return 1.f;

        UINT dpi = GetDpiForWindow(m_pimpl->hwnd); // Window 10+
        return static_cast<float>(dpi) / 96.f;     // 96 = 100%
    }

    Position Window::cursor_position() const
    {
        if (!m_pimpl || !m_pimpl->hwnd) return { 0, 0 };

        POINT pt{};
        GetCursorPos(&pt);
        ScreenToClient(m_pimpl->hwnd, &pt);

        return { pt.x, pt.y };
    }

    void Window::poll_events()
    {
        m_pimpl->event_index = 0;
        m_pimpl->frame_events.clear();

        MSG msg{};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (WM_QUIT == msg.message) {
                m_pimpl->should_close = true;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        m_pimpl->frame_events.swap(m_pimpl->accumulated_events);
    }

    bool Window::has_pending_events()
    {
        if (m_pimpl->event_index >= m_pimpl->frame_events.size()) {
            return false;
        }

        return true;
    }

    bool Window::pop_event(WinEvent& out_event)
    {
        if (m_pimpl->frame_events.empty()) {
            return false;
        }
        out_event = m_pimpl->frame_events[m_pimpl->event_index++];

        return true;
    }

    std::optional<WinEvent> Window::pop_event()
    {
        WinEvent event{};
        if (pop_event(event)) {
            return event;
        }

        return std::nullopt;
    }

    std::span<const WinEvent> Window::frame_events() {
        return m_pimpl->frame_events;
    }

    void Window::set_title(std::string_view title) {
        SetWindowText(m_pimpl->hwnd, std::wstring(title.begin(), title.end()).data());
    }

    void Window::set_size(Size size)
    {
        m_width  = size.width;
        m_height = size.height;
        SetWindowPos(m_pimpl->hwnd, nullptr, 0, 0, size.width, size.height, SWP_NOMOVE | SWP_NOZORDER);
    }

    void Window::request_focus()
    {
        if (m_pimpl && m_pimpl->hwnd) {
            SetFocus(m_pimpl->hwnd);
        }
    }

    void Window::go_fullscreen(bool enable)
    {
        if (!m_pimpl || !m_pimpl->hwnd) return;
        if (m_fullscreen == enable) return; // ya estamos en fullscreen

        HWND hwnd = m_pimpl->hwnd;

        if (enable)
        {
            // Guardar estado
            m_saved_style   = static_cast<uint32_t>(GetWindowLong(hwnd, GWL_STYLE));
            m_saved_exstyle = static_cast<uint32_t>(GetWindowLong(hwnd, GWL_EXSTYLE));
            RECT win_rect{};
            GetWindowRect(hwnd, &win_rect);
            m_saved_rect.left = win_rect.left;
            m_saved_rect.right = win_rect.right;
            m_saved_rect.top = win_rect.top;
            m_saved_rect.bottom = win_rect.bottom;

            // Quitar borde y título
            SetWindowLong(hwnd, GWL_STYLE, m_saved_style & ~(WS_CAPTION | WS_THICKFRAME));
            SetWindowLong(hwnd, GWL_EXSTYLE, m_saved_exstyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

            // Obtener el monitor dónde está la ventana
            HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            GetMonitorInfo(monitor, &mi);

            // Poner la ventana en el tamaño del monitor
            SetWindowPos
            (
                hwnd, HWND_TOP,
                mi.rcMonitor.left, mi.rcMonitor.top,
                mi.rcMonitor.right  - mi.rcMonitor.left,
                mi.rcMonitor.bottom - mi.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED
            );

            m_fullscreen = true;
        }
        else
        {
            SetWindowLong(hwnd, GWL_STYLE, static_cast<DWORD>(m_saved_style));
            SetWindowLong(hwnd, GWL_EXSTYLE, static_cast<DWORD>(m_saved_exstyle));

            SetWindowPos
            (
                hwnd, HWND_NOTOPMOST,
                m_saved_rect.left, m_saved_rect.top,
                m_saved_rect.right - m_saved_rect.left,
                m_saved_rect.bottom - m_saved_rect.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED
            );

            m_fullscreen = false;
        }
    }

    void Window::set_cursor_mode(CursorMode mode)
    {
        if (!m_pimpl || !m_pimpl->hwnd) return;
        if (m_cursor_mode == mode) return;

        HWND hwnd = m_pimpl->hwnd;

        // Restaurar el estado anterior si hacía falta
        if (m_pimpl->cursor_clipped)
        {
            ClipCursor(&m_pimpl->prev_clip_rect);
            m_pimpl->cursor_clipped = false;
        }

        switch (mode)
        {
            using enum CursorMode;
        case Normal:
        {
            while (ShowCursor(TRUE) < 0) {}
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            break;
        }
        case Hidden:
        {
            while (ShowCursor(FALSE) >= 0) {}
            break;
        }
        case Disabled:
        {
            while (ShowCursor(FALSE) >= 0) {}

            GetClipCursor(&m_pimpl->prev_clip_rect);

            RECT client_rect{};
            GetClientRect(hwnd, &client_rect);
            POINT ul = { client_rect.left, client_rect.top };
            POINT lr = { client_rect.right, client_rect.bottom };
            ClientToScreen(hwnd, &ul);
            ClientToScreen(hwnd, &lr);

            RECT clip = { ul.x, ul.y, lr.x, lr.y };
            ClipCursor(&clip);
            m_pimpl->cursor_clipped = true;

            // Centrar el cursor
            int center_x = (client_rect.right - client_rect.left) / 2;
            int center_y = (client_rect.bottom - client_rect.top) / 2;
            set_cursor_position({ center_x, center_y });

            m_pimpl->last_cursor_x = center_x;
            m_pimpl->last_cursor_y = center_y;
            break;
        }
        }

        m_cursor_mode = mode;
        m_pimpl->cursor_mode = mode;
    }

    void Window::set_cursor_position(Position position)
    {
        if (!m_pimpl || !m_pimpl->hwnd) return;

        POINT pt = { position.xpos, position.ypos };
        ClientToScreen(m_pimpl->hwnd, &pt);
        SetCursorPos(pt.x, pt.y);
    }


    LRESULT CALLBACK Window::Impl::StaticWndProc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
    {
        Impl* this_window{ nullptr };

        if (WM_NCCREATE == umsg)
        {
            auto lpcs = reinterpret_cast<LPCREATESTRUCT>(lparam);
            this_window = static_cast<Impl*>(lpcs->lpCreateParams);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this_window));
            this_window->hwnd = hwnd;
        }
        else {
            this_window = reinterpret_cast<Impl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (this_window)
        {
            switch (umsg)
            {
            case WM_CLOSE:
            {
                this_window->should_close = true;
                WinEvent event{ WinEventType::Close };
                this_window->accumulated_events.push_back(event);
                return 0;
            }

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;

            case WM_SIZE:
            {
                const int new_width  = LOWORD(lparam);
                const int new_height = HIWORD(lparam);

                this_window->owner->m_width  = new_width;
                this_window->owner->m_height = new_height;

                WinEvent event{ WinEventType::Resize };
                event.size.width  = new_width;
                event.size.height = new_height;
                this_window->accumulated_events.push_back(event);
                return 0;
            }

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP:
            {
                if (umsg == WM_SYSKEYDOWN)
                {
                    bool isF4 = (wparam == VK_F4);
                    bool isAltPressed = (lparam & (static_cast<size_t>(1) << 29)) != 0;

                    if (isAltPressed && isF4) {
                        break;
                    }
                }

                const bool is_down   = (umsg == WM_KEYDOWN || umsg == WM_SYSKEYDOWN);
                const bool was_down  = (lparam & (static_cast<size_t>(1) << 30)) != 0;
                const bool is_repeat = is_down && was_down;

                ActionState action = ActionState::None;
                if (is_down) {
                    action = is_repeat ? ActionState::Repeat : ActionState::Pressed;
                }
                else {
                    action = ActionState::Released;
                }

                WinEvent event{ action == ActionState::Released ? WinEventType::KeyRelease : (action == ActionState::Pressed ? WinEventType::KeyPress : WinEventType::KeyRepeat) };
                event.key.key = TranslateVirtualKey(static_cast<int>(wparam), lparam);
                event.key.scancode  = (lparam >> 16) & 0xFF;
                event.key.modifiers = GetCurrentModifiers();

                this_window->accumulated_events.push_back(event);
                return 0;
            }

            case WM_MOUSEMOVE:
            {
                const int x = static_cast<short>(LOWORD(lparam));
                const int y = static_cast<short>(HIWORD(lparam));

                WinEvent event{ WinEventType::MouseMove };
                event.mouse_move.xpos = x;
                event.mouse_move.ypos = y;
                this_window->accumulated_events.push_back(event);

                // Si estamos en modo Disabled, recentramos el cursor
                // para que nunca se escape y podamos seguir recibiendo deltas
                if (this_window->cursor_mode == CursorMode::Disabled)
                {
                    const int center_x = this_window->owner->width() / 2;
                    const int center_y = this_window->owner->height() / 2;

                    // Solo recentraremos si se ha movido lo suficiente
                    if (x != center_x || y != center_y) {
                        this_window->owner->set_cursor_position({ center_x, center_y });
                    }
                }

                return 0;
            }

            case WM_MOUSEWHEEL:
            {
                const int delta = GET_WHEEL_DELTA_WPARAM(wparam); // GET_WHEEL_DELTA_WPARAM devuelve múltiplos de WHEEL_DELTA (120)

                WinEvent event{ WinEventType::MouseScroll };
                event.scroll.xoffset = 0.f;
                event.scroll.yoffset = static_cast<float>(delta) / static_cast<float>(WHEEL_DELTA);
                this_window->accumulated_events.push_back(event);
                return 0;
            }

            case WM_MOUSEHWHEEL:   // scroll horizontal (trackpads / ratones de alta gama)
            {
                const int delta = GET_WHEEL_DELTA_WPARAM(wparam);

                WinEvent event{ WinEventType::MouseScroll };
                event.scroll.xoffset = static_cast<float>(delta) / static_cast<float>(WHEEL_DELTA);
                event.scroll.yoffset = 0.0f;
                this_window->accumulated_events.push_back(event);
                return 0;
            }

            case WM_SETFOCUS:
            {
                this_window->owner->m_focused = true;

                WinEvent event{ WinEventType::FocusGained };
                this_window->accumulated_events.push_back(event);
                return 0;
            }

            case WM_KILLFOCUS:
            {
                this_window->owner->m_focused = false;

                WinEvent event{ WinEventType::FocusLost };
                this_window->accumulated_events.push_back(event);
                return 0;
            }

            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
            {
                SetCapture(hwnd);
                WinEvent event{ WinEventType::MouseButtonPress };
                event.mouse_button.ypos = static_cast<short>(HIWORD(lparam));
                event.mouse_button.xpos = static_cast<short>(LOWORD(lparam));
                event.mouse_button.button = [umsg]() {
                    switch (umsg)
                    {
                        using enum MouseButton;
                    case WM_LBUTTONDOWN: return Left;
                    case WM_MBUTTONDOWN: return Middle;
                    case WM_RBUTTONDOWN: return Right;
                    default: return None;
                    }
                }();
                event.mouse_button.action    = ActionState::Pressed;
                event.mouse_button.modifiers = GetCurrentMouseModifiers(wparam);

                this_window->accumulated_events.push_back(event);
                return 0;
            }

            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP:
            {
                ReleaseCapture();
                WinEvent event{ WinEventType::MouseButtonRelease };
                event.mouse_button.xpos = static_cast<short>(LOWORD(lparam));
                event.mouse_button.ypos = static_cast<short>(HIWORD(lparam));
                event.mouse_button.button = [umsg]() {
                    switch (umsg)
                    {
                        using enum MouseButton;
                    case WM_LBUTTONUP: return Left;
                    case WM_MBUTTONUP: return Middle;
                    case WM_RBUTTONUP: return Right;
                    default: return None;
                    }
                }();
                event.mouse_button.action    = ActionState::Released;
                event.mouse_button.modifiers = GetCurrentMouseModifiers(wparam);

                this_window->accumulated_events.push_back(event);
                return 0;
            }

            case WM_CHAR:
            {
                WinEvent event{ WinEventType::CharInput };
                event.text.codepoint = static_cast<char32_t>(wparam);
                this_window->accumulated_events.push_back(event);
                return 0;
            }

            case WM_DPICHANGED:
            {
                const int dpi_x = LOWORD(wparam);
                const int dpi_y = HIWORD(wparam);
                const RECT* suggested = reinterpret_cast<const RECT*>(lparam);

                SetWindowPos
                (
                    hwnd, nullptr,
                    suggested->left, suggested->top,
                    suggested->right - suggested->left,
                    suggested->bottom - suggested->top,
                    SWP_NOOWNERZORDER | SWP_NOACTIVATE
                );

                WinEvent event{ WinEventType::DpiChanged };
                event.dpi.dpi_x = dpi_x;
                event.dpi.dpi_y = dpi_y;
                event.dpi.suggested_rect.left = suggested->left;
                event.dpi.suggested_rect.right = suggested->right;
                event.dpi.suggested_rect.top = suggested->top;
                event.dpi.suggested_rect.bottom = suggested->bottom;
                this_window->accumulated_events.push_back(event);
                return 0;
            }

            default:
                break;
            }
        }

        return DefWindowProc(hwnd, umsg, wparam, lparam);
    }


    static KeyCode TranslateVirtualKey(int virtualKeyCode, LPARAM lparam)
    {
        using enum KeyCode;

        const bool extended = (lparam & (1 << 24)) != 0;

        if (virtualKeyCode >= 'A' && virtualKeyCode <= 'Z') {
            return static_cast<KeyCode>(static_cast<int>(A) + (virtualKeyCode - 'A'));
        }
        if (virtualKeyCode >= '0' && virtualKeyCode <= '9') {
            return static_cast<KeyCode>(static_cast<int>(Num0) + (virtualKeyCode - '0'));
        }
        if (virtualKeyCode >= VK_F1 && virtualKeyCode <= VK_F12) {
            return static_cast<KeyCode>(static_cast<int>(F1) + (virtualKeyCode - VK_F1));
        }

        switch (virtualKeyCode) // NOSONAR - el número de cases es necesario para manejar el teclado completo
        {
        case VK_SHIFT:
        {
            const int scancode = (lparam >> 16) & 0xFF;
            // Scancode 0x2A = Left Shift, 0x36 = Right Shift
            return (scancode == 0x36) ? KeyCode::RightShift : KeyCode::LeftShift;
        }

        case VK_CONTROL:
            return extended ? KeyCode::RightControl : KeyCode::LeftControl;

        case VK_MENU:
            return extended ? KeyCode::RightAlt : KeyCode::LeftAlt;
        case VK_LWIN: return KeyCode::LeftSuper;
        case VK_RWIN: return KeyCode::RightSuper;

            // Navigation and others
        case  VK_ESCAPE: return Escape;
        case     VK_TAB: return Tab;
        case VK_CAPITAL: return CapsLock;
        case   VK_SPACE: return Space;
        case  VK_RETURN: return extended ? KP_Enter : Enter;
        case    VK_BACK: return Backspace;
        case  VK_INSERT: return Insert;
        case  VK_DELETE: return Delete;
        case    VK_HOME: return Home;
        case     VK_END: return End;
        case   VK_PRIOR: return PageUp;   // Page Up
        case    VK_NEXT: return PageDown; // Page Down

        case    VK_LEFT: return LeftArrow;
        case   VK_RIGHT: return RightArrow;
        case      VK_UP: return UpArrow;
        case    VK_DOWN: return DownArrow;

        case VK_SNAPSHOT: return PrintScreen;
        case   VK_SCROLL: return ScrollLock;
        case    VK_PAUSE: return Pause;
        case  VK_NUMLOCK: return NumLock;

        case  VK_NUMPAD0: return KP_0;
        case  VK_NUMPAD1: return KP_1;
        case  VK_NUMPAD2: return KP_2;
        case  VK_NUMPAD3: return KP_3;
        case  VK_NUMPAD4: return KP_4;
        case  VK_NUMPAD5: return KP_5;
        case  VK_NUMPAD6: return KP_6;
        case  VK_NUMPAD7: return KP_7;
        case  VK_NUMPAD8: return KP_8;
        case  VK_NUMPAD9: return KP_9;
        case  VK_DECIMAL: return KP_Decimal;
        case   VK_DIVIDE: return KP_Divide;
        case VK_MULTIPLY: return KP_Multiply;
        case VK_SUBTRACT: return KP_Subtract;
        case      VK_ADD: return KP_Add;

        default:
            return Unknown; // Devuelve Unknown para códigos de teclas no mapeados
        }
    }

    static uint32_t GetCurrentModifiers()
    {
        uint32_t modifiers{ 0 };

        const bool key_state_shift   = GetKeyState(VK_SHIFT  ) & 0x8000;
        const bool key_state_control = GetKeyState(VK_CONTROL) & 0x8000;
        const bool key_state_alt     = GetKeyState(VK_MENU   ) & 0x8000;
        const bool key_state_super   = GetKeyState(VK_LWIN   ) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000;
        const bool key_state_capital = GetKeyState(VK_CAPITAL) & 0x0001;
        const bool key_state_numlock = GetKeyState(VK_NUMLOCK) & 0x0001;
        const bool key_state_scroll  = GetKeyState(VK_SCROLL ) & 0x0001;

        if (key_state_shift)   modifiers |= static_cast<uint32_t>(KeyModifier::Shift);
        if (key_state_control) modifiers |= static_cast<uint32_t>(KeyModifier::Control);
        if (key_state_alt)     modifiers |= static_cast<uint32_t>(KeyModifier::Alt);
        if (key_state_super)   modifiers |= static_cast<uint32_t>(KeyModifier::Super);
        if (key_state_capital) modifiers |= static_cast<uint32_t>(KeyModifier::CapsLock);
        if (key_state_numlock) modifiers |= static_cast<uint32_t>(KeyModifier::NumLock);
        if (key_state_scroll)  modifiers |= static_cast<uint32_t>(KeyModifier::ScrollLock);

        return modifiers;
    }

    static uint32_t GetCurrentMouseModifiers(WPARAM wparam)
    {
        uint32_t modifiers{ 0 };

        const bool key_state_control  = wparam & MK_CONTROL;
        const bool mouse_left_button  = wparam & MK_LBUTTON;
        const bool mouse_mid_button   = wparam & MK_MBUTTON;
        const bool mouse_right_button = wparam & MK_RBUTTON;
        const bool key_state_shift    = wparam & MK_SHIFT;
        const bool mouse_xbut1_button = wparam & MK_XBUTTON1;
        const bool mouse_xbut2_button = wparam & MK_XBUTTON2;

        if (key_state_control)  modifiers |= static_cast<uint32_t>(MouseModifier::Control);
        if (mouse_left_button)  modifiers |= static_cast<uint32_t>(MouseModifier::LeftButton);
        if (mouse_mid_button)   modifiers |= static_cast<uint32_t>(MouseModifier::MiddleButton);
        if (mouse_right_button) modifiers |= static_cast<uint32_t>(MouseModifier::RigthButton);
        if (key_state_shift)    modifiers |= static_cast<uint32_t>(MouseModifier::Shift);
        if (mouse_xbut1_button) modifiers |= static_cast<uint32_t>(MouseModifier::XButton1);
        if (mouse_xbut2_button) modifiers |= static_cast<uint32_t>(MouseModifier::XButton2);

        return modifiers;
    }

} // namespace vitreum

#endif

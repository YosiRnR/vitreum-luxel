#pragma once
#include "types.h"
#include <span>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <filesystem>

namespace vitreum
{
	struct WinEvent;
	class Window;

	struct WindowDescriptor
	{
		std::string title{ "Window" };

		Size size;
		Position position;
	};

	struct FileDialogFilter
	{
		std::string name{};
		std::string pattern{ "*.*" };
	};

	struct FileDialogOptions
	{
		std::string title{ "Abrir archivo" };
		std::vector<FileDialogFilter> filters{};
		std::filesystem::path initial_directory{};
	};

	std::optional<std::filesystem::path> open_file_dialog(
			const Window& owner,
			const FileDialogOptions options = {});

	enum class CursorMode : uint8_t
	{
		Normal,  // Visible y libre
		Hidden,  // Invisible pero se puede mover libremente
		Disabled // Invisible + capturado (ideal para cámara FPS)
	};

	class Window
	{
		public:
			explicit Window();
			~Window();

			// Eliminar copias y movimientos
			Window(const Window&) = delete;
			Window(const Window&&) = delete;
			Window& operator=(Window&) = delete;
			Window& operator=(Window&&) = delete;

			// Interface
			bool create(const WindowDescriptor&);
			void show() const;
			void poll_events();
			bool should_close() const;

			bool pop_event(WinEvent&);
			std::optional<WinEvent> pop_event();
			bool has_pending_events();

			// Setters
			void set_title(std::string_view title);
			void  set_size(Size size);
			void request_focus();
			void go_fullscreen(bool enable);
			void set_cursor_mode(CursorMode mode);
			void set_cursor_position(Position position);
			void set_confirm_close(bool close);

			// Getters
			NativeWindowHandle native_handle() const;
			int  width() const { return m_width; }
			int height() const { return m_height; }
			float content_scale() const;
			CursorMode   cursor_mode() const { return m_cursor_mode; }
			Position cursor_position() const;

			bool is_focused() const { return m_focused; }
			bool is_fullscreen() const { return m_fullscreen; }

			std::span<const WinEvent> frame_events();

		private:
			struct Impl;
			std::unique_ptr<Impl> m_pimpl{ nullptr };

			int m_width;
			int m_height;

			bool m_focused{ false };
			bool m_fullscreen{ false };
			CursorMode m_cursor_mode{ CursorMode::Normal };

			// Para poder restaurar al salir de m_fullscreen
			uint32_t m_saved_style{ 0 };
			uint32_t m_saved_exstyle{ 0 };
			Rect m_saved_rect{};
	};

} // namespace vitreum


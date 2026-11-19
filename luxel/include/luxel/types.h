#pragma once

#include <concepts>
#include <utility>
#include <cstdint>
#include <atomic>

namespace luxel
{
	enum class PlatformType : uint8_t { Unknown = 0, Windows, Linux };

	struct Win32Handle
	{
		void* hwnd{ nullptr };      // Puntero opaco para HWND
		void* hinstance{ nullptr }; // Puntero opaco para HINSTANCE
	};

	struct LinuxHandle
	{
		void* display{ nullptr };
		uint32_t window{};
	};

	struct WindowHandle
	{
		PlatformType platform{ PlatformType::Unknown };

		union
		{
			Win32Handle windows;
			LinuxHandle linux;
		};
	};

	struct Size2D
	{
		uint32_t  width{};
		uint32_t height{};
	};

	// Formatos portables. Cada backend los traduce a su equivalente nativo.
	enum class Format : uint8_t
	{
		Undefined = 0,
		R8G8B8A8_UNorm,
		R8G8B8A8_SRGB,
		B8G8R8A8_UNorm,
		B8G8R8A8_SRGB,
		D32_Float,
		D24_UNorm_S8_UInt
	};
	inline constexpr uint8_t format_bytes_per_pixel(Format format)
	{
		switch (format)
		{
		case Format::R8G8B8A8_UNorm:    return 4;
		case Format::R8G8B8A8_SRGB:     return 4;
		case Format::B8G8R8A8_UNorm:    return 4;
		case Format::B8G8R8A8_SRGB:     return 4;
		case Format::D32_Float:         return 4;
		case Format::D24_UNorm_S8_UInt: return 4;
		case Format::Undefined:         return 0;
		}

		return 0;
	}

	enum class PrimitiveTopology : uint8_t
	{
		TriangleList,
		TriangleStrip,
		LineList,
		LineStrip,
		PointList
	};

	class RefCounted
	{
		mutable std::atomic<uint32_t> m_ref_count{ 0 }; // atomic para funcionar entre hilos

	public:
		RefCounted() = default;
		virtual ~RefCounted() = default;

		// Deshabilitamos copia/movimiento para no duplicar contadores por error
		RefCounted(const RefCounted&) = delete;
		RefCounted& operator=(const RefCounted&) = delete;

		void add_ref() const noexcept {
			m_ref_count.fetch_add(1, std::memory_order_relaxed);
		}

		void release() const noexcept {
			// Decrementamos el contador y, si llega a 0, destruimos el objeto
			// NOLINTNEXTLINE(cppcoreguidelines-owning-memory) 
			if (m_ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
				on_zero_refs();
			}
		}

		uint32_t ref_count() const noexcept {
			return m_ref_count.load(std::memory_order_relaxed);
		}

	protected:
		virtual void on_zero_refs() const {
			delete this;
		}
	};

	template<class T>
	concept IsRefCounted = std::derived_from<T, RefCounted>;

	template<IsRefCounted T>
	class Ref
	{
		template<IsRefCounted U> friend class Ref;

		T* m_ptr{ nullptr };

	public:
		// 1. Constructores por defecto y desde puntero crudo
		Ref() = default;
		explicit Ref(T* ptr) : m_ptr(ptr)
		{
			if (m_ptr) {
				m_ptr->add_ref();
			}
		}

		// 2. Destructor (RAII: decrementa referencia al salir de ámbito)
		~Ref()
		{
			if (m_ptr) {
				m_ptr->release();
			}
		}

		// 3. Constructor de copia (Suma referencia)
		Ref(const Ref<T>& other) : m_ptr(other.m_ptr)
		{
			if (m_ptr) {
				m_ptr->add_ref();
			}
		}

		// Constructor de copia desde clase derivada (sin explicit para permitir conversiones implícitas)
		template<class U> requires std::derived_from<U, T>
		Ref(const Ref<U>& other) : m_ptr(other.m_ptr)
		{
			if (m_ptr) {
				m_ptr->add_ref();
			}
		}

		// 4. Operador de asignación por copia
		Ref<T>& operator=(const Ref<T>& other)
		{
			if (this->m_ptr != other.m_ptr) {
				if (m_ptr) {
					m_ptr->release(); // Liberamos la referencia actual
				}
				m_ptr = other.m_ptr;
				if (m_ptr) {
					m_ptr->add_ref(); // Añadimos la nueva
				}
			}
			return *this;
		}

		template<class U> requires std::derived_from<U, T>
		Ref<T>& operator=(const Ref<U>& other)
		{
			if (T* new_ptr = other.m_ptr; this->m_ptr != new_ptr)
			{
				if (m_ptr) {
					m_ptr->release();
				}
				m_ptr = new_ptr;
				if (m_ptr) {
					m_ptr->add_ref();
				}
			}
			return *this;
		}

		// 5. Constructor y asignación por movimiento (Súper eficiente: 0 incrementos atómicos)
		Ref(Ref<T>&& other) noexcept : m_ptr(other.m_ptr) {
			other.m_ptr = nullptr;
		}

		// Constructor de movimiento desde clase derivada (sin explicit para permitir conversiones implicitas)
		template<class U> requires std::derived_from<U, T>
		Ref(Ref<U>&& other) noexcept : m_ptr(std::exchange(other.m_ptr, nullptr)) {
		}


		Ref<T>& operator=(Ref<T>&& other) noexcept
		{
			if (this != &other)
			{
				if (m_ptr) {
					m_ptr->release();
				}

				m_ptr = std::exchange(other.m_ptr, nullptr);
			}

			return *this;
		}

		template<class U> requires std::derived_from<U, T>
		Ref<T>& operator=(Ref<U>&& other) noexcept
		{
			if (m_ptr != other.m_ptr)
			{
				if (m_ptr) {
					m_ptr->release();
				}
				m_ptr = other.m_ptr;
				other.m_ptr = nullptr;
			}
			return *this;
		}

		// 6. Sobrecarga de operadores de puntero
		T* operator->() const noexcept { return m_ptr; }
		T& operator*()  const noexcept { return *m_ptr; }
		T* get() const noexcept { return m_ptr; }

		explicit operator bool() const noexcept { return m_ptr != nullptr; }
	};

} // namespace luxel

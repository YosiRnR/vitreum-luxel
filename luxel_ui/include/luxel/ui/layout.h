#pragma once

// Contenedores: widgets que no se pintan y colocan a sus hijos (D3 de
// docs/ui-design.md). Todos ignoran el ratón (sus hijos no).

#include <luxel/ui/widget.h>

namespace luxel::ui
{
	// Espacio libre por los cuatro lados, en píxeles.
	struct Insets
	{
		float left{ 0.0f };
		float top{ 0.0f };
		float right{ 0.0f };
		float bottom{ 0.0f };

		static Insets all(float value) { return { value, value, value, value }; }
	};

	// Unos encima de otros: todos los hijos comparten el mismo hueco, y cada uno se
	// coloca en él según su set_align(). El último se pinta encima.
	//
	//     screen.add<Panel>(style).set_align(Placement::End, Placement::End);        // abajo a la derecha
	//     screen.add<Panel>(style).set_align(Placement::Center, Placement::Center);  // en el centro
	class Stack : public Widget
	{
	public:
		Stack() { set_ignores_mouse(true); }

		const Insets& padding() const { return m_padding; }
		void set_padding(const Insets& padding) { m_padding = padding; }

	protected:
		math::Vector2f measure_content(const Theme& theme) override;
		void arrange_children() override;

	private:
		Insets m_padding{};
	};

	// Los hijos en fila (Row) o en columna (Column), uno detrás de otro.
	//
	// A lo largo (en Column, de arriba abajo): cada uno mide lo que pide, y lo que
	// sobra se reparte entre los que tienen set_grow(). Si no cabe, se sale por el
	// final: no se encoge a nadie.
	// A lo ancho: según su set_align (por defecto, Fill: todo el ancho).
	class Linear : public Widget
	{
	public:
		const Insets& padding() const { return m_padding; }
		void set_padding(const Insets& padding) { m_padding = padding; }

		// Separación entre hijos (no antes del primero ni después del último).
		float gap() const { return m_gap; }
		void set_gap(float gap) { m_gap = gap; }

	protected:
		explicit Linear(bool vertical) : m_vertical(vertical) { set_ignores_mouse(true); }

		math::Vector2f measure_content(const Theme& theme) override;
		void arrange_children() override;

	private:
		bool m_vertical;
		Insets m_padding{};
		float m_gap{ 0.0f };
	};

	class Row : public Linear
	{
	public:
		Row() : Linear(false) {}
	};

	class Column : public Linear
	{
	public:
		Column() : Linear(true) {}
	};

	// Un hueco que crece: empuja a los demás hacia los lados.
	//     [Aceptar] [......Spacer......] [Cancelar]
	class Spacer : public Widget
	{
	public:
		Spacer() { set_ignores_mouse(true); set_grow(1.0f); }
	};

} // namespace luxel::ui

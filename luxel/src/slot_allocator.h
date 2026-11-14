#pragma once

#include <mutex>
#include <vector>
#include <cstdint>
#include <cassert>

namespace luxel
{
	class SlotAllocator
	{
	public:
		explicit SlotAllocator(uint32_t capacity)
			: m_capacity(capacity)
		{
			m_free_list.reserve(capacity); // Evita reallocates cuando entren elementos al m_free_list
#ifndef NDEBUG
			m_occupied.resize(capacity, 0);
#endif
		}

		// Se devuelve 0 en caso de no haber índices libres
		uint32_t allocate()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			uint32_t index{};
			if (!m_free_list.empty())
			{
				index = m_free_list.back();
				m_free_list.pop_back();
			}
			else
			{
				if (m_free_index >= m_capacity) {
					// No puede superar la capacidad, la capacidad es un tope fijo. Devolvemos 0 como indicador de inválido
					return 0;
				}
				index = m_free_index++;
			}
#ifndef NDEBUG
			assert(index < m_capacity);
			assert(m_occupied[index] == 0 && "Double allocate!!");
			m_occupied[index] = 1;
#endif
			return index;
		}

		void free(uint32_t index)
		{
			if (index == 0) return; // No liberamos el valor que representa inválido

			std::lock_guard<std::mutex> lock(m_mutex);
#ifndef NDEBUG
			assert(index < m_capacity);
			assert(m_occupied[index] == 1 && "Double free!!");
			m_occupied[index] = 0;
#endif
			m_free_list.push_back(index);
		}

	private:
		std::vector<uint32_t> m_free_list{};
		uint32_t m_capacity{};
		uint32_t m_free_index{ 1 };
		std::mutex m_mutex;

#ifndef NDEBUG
		std::vector<uint8_t> m_occupied{};
#endif
	};

} // namespace luxel

#include <slot_allocator.h>
#include <Windows.h>
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <atomic>
#include <thread>

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* what, int line)
	{
		if (!condition) {
			++g_failures;
			std::cout << "  FALLO (linea " << line << "): " << what << "\n";
		}
	}
}

#define CHECK(cond) check((cond), #cond, __LINE__)

// 1. Los primeros índices concedidos son distintos entre sí y ninguno es 0.
static void test_first_indices_are_different_and_not_zero(int samples)
{
	std::cout << "[1] primeros índices concedidos son distintos entre sí y ninguno es 0\n";
	luxel::SlotAllocator allocator{ 2048 };
	std::vector<uint32_t> indices;
	indices.reserve(samples);
	for (int i = 0; i < samples; ++i) {
		indices.push_back(allocator.allocate());
	}
	std::sort(indices.begin(), indices.end());
	bool success{ indices[0] != 0 };
	for (size_t i = 0; i < indices.size() - 1; ++i)
	{
		if (indices[i + 1] != 0 && indices[i] != indices[i + 1]) {
			continue;
		}
		success = false;
	}

	CHECK(success);
}

// 2. Tras liberar uno, el siguiente allocate() lo devuelve.
static void test_after_free_allocate_returns_the_same()
{
	std::cout << "[2] Tras liberar uno, el siguiente allocate() lo devuelve\n";
	luxel::SlotAllocator allocator{ 2048 };
	uint32_t index1 = allocator.allocate();
	allocator.free(index1);
	uint32_t index2 = allocator.allocate();

	CHECK(index1 == index2);
}

// 3. Con capacidad pequeña (por ejemplo 4), se agota en el número esperado y devuelve el valor de error.
static void test_out_of_capacity_and_return_error_code()
{
	std::cout << "[3] Con capacidad pequeña (por ejemplo 4), se agota en el número esperado y devuelve el valor de error\n";
	luxel::SlotAllocator allocator{ 4 };
	int brk_position{ 0 };
	for (int i = 0; i < 10; ++i)
	{
		uint32_t index = allocator.allocate();
		if (index == 0)
		{
			brk_position = i;
			break;
		}
	}

	CHECK(brk_position == 3);
}

// 4. Liberar el 0 no hace nada.
static void test_free_zero()
{
	std::cout << "[4] Liberar el 0 no hace nada\n";
	luxel::SlotAllocator allocator{ 4 };
	uint32_t index1 = allocator.allocate();
	allocator.free(0);
	uint32_t index2 = allocator.allocate();

	CHECK(index1 != index2 && index2 != 0);
}

// 5. Con varios hilos haciendo allocate/free a la vez, nunca se concede un mismo índice a dos hilos a la vez.
static void test_threads_never_share_an_index()
{
	std::cout << "[5] Varios hilos: nunca se concede el mismo índice a dos hilos a la vez\n";

	constexpr uint32_t k_threads = 8;
	constexpr uint32_t k_held_per_thread = 16;  // índices que cada hilo tiene vivos a la vez
	constexpr uint32_t k_rounds = 2000;
	// +1 porque el 0 está reservado. Así nunca se agota y un 0 devuelto sería un fallo real
	luxel::SlotAllocator allocator{ k_threads * k_held_per_thread + 1 };

	// owned[i] vale 1 mientras algún hilo posee el índice i. Si allocate() entregara
	// el mismo índice a dos hilos, el segundo exchange(1) devolvería 1.
	std::vector<std::atomic<uint8_t>> owned(k_threads * k_held_per_thread + 1);
	std::atomic<int> failures{ 0 };

	{
		std::vector<std::jthread> threads; // jthread hace join al destruirse, al salir de este bloque
		for (uint32_t t = 0; t < k_threads; ++t)
		{
			threads.emplace_back([&]
			{
				std::vector<uint32_t> mine;
				mine.reserve(k_held_per_thread);
				for (uint32_t round = 0; round < k_rounds; ++round)
				{
					for (uint32_t i = 0; i < k_held_per_thread; ++i)
					{
						uint32_t index = allocator.allocate();
						if (index == 0 || owned[index].exchange(1) != 0) {
							++failures;
						}
						mine.push_back(index);
					}
					for (uint32_t index : mine)
					{
						// Primero se marca como libre y después se devuelve, para que otro hilo
						// no pueda recibirlo mientras aún figura como ocupado
						if (index != 0) {
							owned[index].store(0);
							allocator.free(index);
						}
					}
					mine.clear();
				}
			});
		}
	}

	CHECK(failures.load() == 0);
}

int main()
{
	SetConsoleOutputCP(CP_UTF8);
	system("cls");

	test_first_indices_are_different_and_not_zero(20);
	test_after_free_allocate_returns_the_same();
	test_out_of_capacity_and_return_error_code();
	test_free_zero();
	test_threads_never_share_an_index();

	std::cout << (g_failures == 0 ? "\nTODO OK\n" : "\nHAY FALLOS\n");
	return g_failures == 0 ? 0 : 1;
}

#include <slot_allocator.h>
#include <free_list_allocator.h>
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

// --- TESTS DE FreeListAllocator ---

// 6. Básico: bloque de 100; allocate(30), allocate(20), allocate(25) dan offsets 0, 30 y 50.
static void test_freelist_basic_allocations()
{
	std::cout << "[6] FreeList básico: offsets consecutivos 0, 30 y 50\n";
	luxel::FreeListAllocator allocator{ 100 };

	auto a = allocator.allocate(30, 1);
	auto b = allocator.allocate(20, 1);
	auto c = allocator.allocate(25, 1);

	CHECK(a.has_value() && a->offset == 0 && a->size == 30);
	CHECK(b.has_value() && b->offset == 30 && b->size == 20);
	CHECK(c.has_value() && c->offset == 50 && c->size == 25);
	CHECK(allocator.free_size() == 25);
	CHECK(!allocator.is_empty());
}

// 7. Fusión por ambos lados: libera la del medio, luego la última, luego la primera.
static void test_freelist_coalesce_middle_last_first()
{
	std::cout << "[7] FreeList fusión: libera medio, última y primera -> recupera 100\n";
	luxel::FreeListAllocator allocator{ 100 };

	auto a = allocator.allocate(30, 1); // [0..30]
	auto b = allocator.allocate(20, 1); // [30..50]
	auto c = allocator.allocate(25, 1); // [50..75] (queda hueco [75..100])

	// Liberamos la del medio: hueco aislado [30..50]
	allocator.free(b->offset, b->size);
	CHECK(allocator.free_size() == 45);

	// Liberamos la última: se fusiona con el hueco del medio y el final -> [30..100]
	allocator.free(c->offset, c->size);
	CHECK(allocator.free_size() == 70);

	// Liberamos la primera: se fusiona todo -> [0..100]
	allocator.free(a->offset, a->size);
	CHECK(allocator.free_size() == 100);
	CHECK(allocator.is_empty());
}

// 8. Fusión izquierda y derecha en un solo free: libera A y C, y por último B.
static void test_freelist_coalesce_bridge_both_sides()
{
	std::cout << "[8] FreeList puente: libera A y C, luego B une los tres\n";
	luxel::FreeListAllocator allocator{ 100 };

	auto a = allocator.allocate(30, 1); // [0..30]
	auto b = allocator.allocate(20, 1); // [30..50]
	auto c = allocator.allocate(50, 1); // [50..100] (bloque entero lleno)

	CHECK(allocator.free_size() == 0);

	// Liberamos extremos A y C (quedan dos huecos separados por B)
	allocator.free(a->offset, a->size);
	allocator.free(c->offset, c->size);
	CHECK(allocator.free_size() == 80);
	CHECK(!allocator.is_empty());

	// Liberamos B en medio: puentea A y C en un solo hueco de 100
	allocator.free(b->offset, b->size);
	CHECK(allocator.free_size() == 100);
	CHECK(allocator.is_empty());
}

// 9. Alineación: allocate(30, 1) y allocate(20, 64) dan offset 64 y al liberar se recupera todo.
static void test_freelist_alignment_and_recovery()
{
	std::cout << "[9] FreeList alineación: offset 64 y recuperación completa del hueco\n";
	luxel::FreeListAllocator allocator{ 100 };

	auto a = allocator.allocate(30, 1);  // [0..30]
	auto b = allocator.allocate(20, 64); // debe saltar a 64 -> [64..84], dejando padding [30..64] (34B) y resto [84..100] (16B)

	CHECK(a.has_value() && a->offset == 0);
	CHECK(b.has_value() && b->offset == 64 && b->size == 20);
	CHECK(allocator.free_size() == 50); // 34 + 16 = 50 libres

	// Al liberar B, el hueco de 34 + B(20) + 16 debe unirse en un hueco continuo de 70 [30..100]
	allocator.free(b->offset, b->size);
	CHECK(allocator.free_size() == 70);

	// Comprobamos que ese hueco de 70 es realmente continuo pidiendo 70 de golpe
	auto big = allocator.allocate(70, 1);
	CHECK(big.has_value() && big->offset == 30 && big->size == 70);

	// Liberamos todo y verificamos que queda vacío
	allocator.free(big->offset, big->size);
	allocator.free(a->offset, a->size);
	CHECK(allocator.is_empty());
}

// 10. Lleno y protección contra overflow: pedir más de lo que cabe o UINT64_MAX devuelve nullopt.
static void test_freelist_out_of_memory_and_overflow()
{
	std::cout << "[10] FreeList lleno y protección contra overflow (UINT64_MAX)\n";
	luxel::FreeListAllocator allocator{ 100 };

	auto too_big = allocator.allocate(101, 1);
	auto overflow = allocator.allocate(UINT64_MAX, 64);

	CHECK(!too_big.has_value());
	CHECK(!overflow.has_value());

	// Llenamos parcialmente (60 bytes) y pedimos 50 con alineación (no cabe)
	auto a = allocator.allocate(60, 1);
	auto b = allocator.allocate(50, 1);
	CHECK(a.has_value());
	CHECK(!b.has_value());
}

// 11. Reutilización: tras liberar, volver a pedir lo mismo da el mismo offset.
static void test_freelist_reuse_freed_range()
{
	std::cout << "[11] FreeList reutilización: tras liberar devuelve el mismo offset\n";
	luxel::FreeListAllocator allocator{ 100 };

	auto a = allocator.allocate(20, 1); // [0..20]
	auto b1 = allocator.allocate(30, 1); // [20..50]
	auto c = allocator.allocate(20, 1); // [50..70]

	CHECK(b1.has_value() && b1->offset == 20);

	allocator.free(b1->offset, b1->size);

	// Volvemos a pedir exactamente 30: debe encajar en el hueco de B1 en offset 20
	auto b2 = allocator.allocate(30, 1);
	CHECK(b2.has_value() && b2->offset == 20 && b2->size == 30);

	allocator.free(a->offset, a->size);
	allocator.free(b2->offset, b2->size);
	allocator.free(c->offset, c->size);
	CHECK(allocator.is_empty());
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

	std::cout << "\n--- FreeListAllocator ---\n";
	test_freelist_basic_allocations();
	test_freelist_coalesce_middle_last_first();
	test_freelist_coalesce_bridge_both_sides();
	test_freelist_alignment_and_recovery();
	test_freelist_out_of_memory_and_overflow();
	test_freelist_reuse_freed_range();

	std::cout << (g_failures == 0 ? "\nTODO OK\n" : "\nHAY FALLOS\n");
	return g_failures == 0 ? 0 : 1;
}

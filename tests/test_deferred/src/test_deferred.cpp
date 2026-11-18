// Pruebas de la destruccion diferida (hito 4).
//
// Es un test de "caja blanca": incluye cabeceras privadas de luxel (ver el
// CMakeLists de esta carpeta) para poder encolar un recurso en el VulkanDevice.
// Cuando existan Buffer/Texture reales, esta clase TestResource desaparece:
// los recursos reales harian exactamente lo mismo en su on_zero_refs().

#include <vitreum/vitreum.h>
#include <luxel/luxel.h>
#include "backends/vulkan/vulkan_device.h"
#include "backends/vulkan/vulkan_frame_allocator.h"

#include <filesystem>
#include <stdexcept>
#include <iostream>
#include <utility>
#include <cstdint>
#include <fstream>
#include <vector>
#include <span>

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

// Recurso minimo con destruccion diferida. Pone *destroyed a true cuando se
// ejecuta su destructor (es decir, cuando la cola de borrado lo elimina).
class TestResource : public luxel::RefCounted
{
public:
	TestResource(luxel::Ref<luxel::Device> device, bool* destroyed)
		: m_device(std::move(device)), m_destroyed(destroyed) {}

	~TestResource() override { *m_destroyed = true; }

protected:
	void on_zero_refs() const override
	{
		auto* self = const_cast<TestResource*>(this);

		// Sacamos el Ref del miembro a un local ANTES de encolar: ese local debe
		// ser lo ultimo que se destruye, porque si era la ultima referencia al
		// device, su destructor vaciara la cola y hara delete de "this".
		luxel::Ref<luxel::Device> device = std::move(self->m_device);
		static_cast<luxel::vulkan::VulkanDevice*>(device.get())->enqueue_destroy(self);
	}

private:
	luxel::Ref<luxel::Device> m_device;
	bool* m_destroyed;
};

static luxel::Ref<luxel::Device> make_device()
{
	return luxel::create_device({ .backend = luxel::Backend::Vulkan, .debug = true });
}

// Entorno con ventana y swapchain para poder ejecutar frames.
// La ventana se recibe de fuera porque vitreum todavia no permite crear dos
// ventanas en el mismo proceso (la clase de ventana se registra en cada create).
struct FrameRig
{
	luxel::Ref<luxel::Device> device = make_device();
	luxel::Ref<luxel::Swapchain> swapchain;

	explicit FrameRig(vitreum::Window& window)
	{
		luxel::WindowHandle handle{};
		auto native = window.native_handle();
		handle.platform = luxel::PlatformType::Windows;
		handle.windows.hinstance = native.win32.hinstance;
		handle.windows.hwnd = native.win32.hwnd;

		swapchain = device->create_swapchain({
			.window = handle,
			.size = { 640, 480 },
			.vsync = true
		});
	}

	// Antes de destruir la swapchain hay que dejar la GPU quieta.
	~FrameRig() { device->wait_idle(); }

	// Un frame completo (begin + clear + end). Devuelve false si no se pudo pintar.
	bool run_frame()
	{
		auto* frame = device->begin_frame(*swapchain);
		if (frame == nullptr) {
			return false;
		}
		frame->commands().begin_rendering({ .clear = { 0.0f, 0.0f, 0.0f, 1.0f } });
		frame->commands().end_rendering();
		device->end_frame(*frame);
		return true;
	}

	// Solo begin_frame (para soltar el recurso "durante" el frame).
	luxel::Frame* begin() { return device->begin_frame(*swapchain); }
	void end(luxel::Frame& f) { device->end_frame(f); }
};

static std::vector<uint32_t> read_binary_file(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file) {
		throw std::runtime_error("No se pudo abrir el archivo: " + path.string());
	}

	const auto size = static_cast<size_t>(file.tellg());
	if (size % 4 != 0) {
		throw std::runtime_error("El tamaño del fichero SPV de shaders debe ser múltiplo de 4 para ser correcto");
	}
	std::vector<uint32_t> data(size / 4);
	file.seekg(0);
	file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
	return data;
}

// 1. Soltar el ultimo Ref no destruye en el acto: va a la cola.
static void test_not_destroyed_immediately()
{
	std::cout << "[1] no se destruye al soltar el ultimo Ref\n";
	bool destroyed = false;
	auto device = make_device();
	{
		luxel::Ref<TestResource> res(new TestResource(device, &destroyed));
	}
	CHECK(!destroyed);
}

// 2. Si el Ref del recurso era lo ultimo que mantenia vivo al device, el
//    destructor del device debe vaciar la cola (sin fugas, sin doble release).
static void test_device_destroyed_by_last_resource()
{
	std::cout << "[2] el ultimo Ref es el del recurso: el device vacia la cola al morir\n";
	bool destroyed = false;
	auto device = make_device();
	luxel::Ref<TestResource> res(new TestResource(device, &destroyed));

	device = {};   // el usuario suelta el device; lo retiene el recurso
	CHECK(!destroyed);
	res = {};      // el recurso se encola, suelta el device, el device muere y vacia la cola
	CHECK(destroyed);
}

// 3. Soltado ENTRE frames: el frame anterior (N-1) puede seguir en la GPU.
//    No debe destruirse hasta que ese frame haya terminado (2 begin_frame despues).
static void test_released_between_frames(vitreum::Window& window)
{
	std::cout << "[3] soltado entre frames: espera a que acabe el ultimo frame enviado\n";
	bool destroyed = false;
	FrameRig rig(window);
	luxel::Ref<TestResource> res(new TestResource(rig.device, &destroyed));

	CHECK(rig.run_frame());   // frame 0: podria estar usando el recurso
	res = {};                 // se suelta con el frame 0 aun posiblemente en vuelo
	CHECK(!destroyed);

	CHECK(rig.run_frame());   // frame 1: su begin_frame solo espera al frame -1
	CHECK(!destroyed);        // <- el frame 0 todavia puede estar en la GPU

	CHECK(rig.run_frame());   // frame 2: su begin_frame espera al frame 0
	CHECK(destroyed);
}

// 4. Soltado DURANTE un frame: ese mismo frame puede estar usandolo.
static void test_released_during_frame(vitreum::Window& window)
{
	std::cout << "[4] soltado durante un frame: espera a que ese frame termine\n";
	bool destroyed = false;
	FrameRig rig(window);
	luxel::Ref<TestResource> res(new TestResource(rig.device, &destroyed));

	auto* frame = rig.begin();
	CHECK(frame != nullptr);
	res = {};
	CHECK(!destroyed);
	if (frame) { rig.end(*frame); }

	CHECK(rig.run_frame());
	CHECK(!destroyed);

	CHECK(rig.run_frame());
	CHECK(destroyed);
}

// 5. Solicitar huecos de Sampler en el Descriptor de Samplers
static void test_descriptor_set_sampler_slots(vitreum::Window& window)
{
	std::cout << "[5] solicitar huecos de Sampler en el descriptor de samplers\n";
	FrameRig rig(window);

	auto* frame = rig.begin();
	CHECK(frame != nullptr);

	vk::SamplerCreateInfo sampler_info{

	};
	vk::raii::Sampler test_sampler = vk::raii::Sampler(static_cast<luxel::vulkan::VulkanDevice*>(rig.device.get())->logical_device(), sampler_info);
	uint32_t slot1 = static_cast<luxel::vulkan::VulkanDevice*>(rig.device.get())->bindless_table().allocate_sampler_slot(*test_sampler);
	CHECK(slot1 != 0);
	uint32_t slot2 = static_cast<luxel::vulkan::VulkanDevice*>(rig.device.get())->bindless_table().allocate_sampler_slot(*test_sampler);
	CHECK(slot2 != 0 && slot1 != slot2);
	static_cast<luxel::vulkan::VulkanDevice*>(rig.device.get())->bindless_table().free_sampler_slot(slot1);
	uint32_t slot3 = static_cast<luxel::vulkan::VulkanDevice*>(rig.device.get())->bindless_table().allocate_sampler_slot(*test_sampler);
	CHECK(slot3 == slot1);

	if (frame) { rig.end(*frame); }

	CHECK(rig.run_frame());

	CHECK(rig.run_frame());
}

// 6. Crear y soltar Pipeline
static void test_pipeline()
{
	std::cout << "[6] crear y soltar pipeline\n";
	auto device = make_device();
	{
		auto vs_code = read_binary_file(std::filesystem::path(LUXEL_SHADER_DIR) / "shader_vs.spv");
		auto ps_code = read_binary_file(std::filesystem::path(LUXEL_SHADER_DIR) / "shader_ps.spv");
		auto pipeline = static_cast<luxel::vulkan::VulkanDevice*>(device.get())->create_graphics_pipeline({
			.vs_byte_code = std::span<uint32_t>(vs_code),
			.ps_byte_code = std::span<uint32_t>(ps_code),
			.format = luxel::Format::B8G8R8A8_SRGB
		});
	}
	CHECK(device->ref_count() == 1);
}

// 7. Buffer Storage: tiene hueco bindless y este se devuelve de forma diferida.
static void test_buffer_storage_slot(vitreum::Window& window)
{
	std::cout << "[7] buffer Storage: hueco bindless con liberacion diferida\n";
	FrameRig rig(window);

	const std::vector<float> data(64, 1.0f);
	auto desc = luxel::BufferDescriptor::from_data(data, luxel::BufferUsage::Storage);

	auto b1 = rig.device->create_buffer(desc);
	CHECK(b1->size() == data.size() * sizeof(float));
	CHECK(luxel::has_flag(b1->usage(), luxel::BufferUsage::Storage));
	const uint32_t idx1 = b1->bindless_index();
	CHECK(idx1 != 0);

	b1 = {};   // se encola; el hueco NO debe estar libre todavia

	auto b2 = rig.device->create_buffer(desc);
	const uint32_t idx2 = b2->bindless_index();
	CHECK(idx2 != 0);
	CHECK(idx2 != idx1);   // si fuese igual, el hueco se habria liberado antes de tiempo

	b2 = {};
	CHECK(rig.run_frame());
	CHECK(rig.run_frame());
	CHECK(rig.run_frame());   // ya han pasado los frames en vuelo: ambos huecos libres

	auto b3 = rig.device->create_buffer(desc);
	const uint32_t idx3 = b3->bindless_index();
	CHECK(idx3 == idx1 || idx3 == idx2);   // reutiliza un hueco liberado
}

// 8. Buffer solo Index: sin hueco bindless. Tambien comprueba que soltarlo no
//    deja al device retenido.
static void test_buffer_index_no_slot()
{
	std::cout << "[8] buffer Index: sin hueco bindless y sin retener el device\n";
	auto device = make_device();
	{
		const std::vector<uint16_t> indices{ 0, 1, 2 };
		auto buffer = device->create_buffer(luxel::BufferDescriptor::from_data(indices, luxel::BufferUsage::Index));
		CHECK(buffer->bindless_index() == 0);
		CHECK(buffer->size() == indices.size() * sizeof(uint16_t));
		CHECK(luxel::has_flag(buffer->usage(), luxel::BufferUsage::Index));
		CHECK(!luxel::has_flag(buffer->usage(), luxel::BufferUsage::Storage));
	}
	CHECK(device->ref_count() == 1);
}

// 9. Descriptores invalidos: create_buffer debe lanzar.
static void test_buffer_invalid_descriptors()
{
	std::cout << "[9] create_buffer rechaza descriptores invalidos\n";
	auto device = make_device();

	auto throws = [&](const luxel::BufferDescriptor& desc) {
		try {
			device->create_buffer(desc);
		}
		catch (const std::exception&) {
			return true;
		}
		return false;
	};

	CHECK(throws({ .size = 0, .usage = luxel::BufferUsage::Storage }));
	CHECK(throws({ .size = 16, .usage = luxel::BufferUsage::None }));

	const std::vector<std::byte> too_much(32);
	CHECK(throws({ .size = 16, .usage = luxel::BufferUsage::Storage, .initial_data = too_much }));

	// Y uno valido sin datos iniciales no debe lanzar.
	CHECK(!throws({ .size = 16, .usage = luxel::BufferUsage::Storage }));
	CHECK(device->ref_count() == 1);
}

// 10. Subida inicial por staging. No se puede leer el contenido de vuelta (el
//     buffer no es fuente de copia), asi que se comprueba que no lanza, que los
//     huecos son los esperados y que no quedan referencias colgando; los avisos
//     de validacion (consola) son la parte importante de esta prueba.
static void test_buffer_upload()
{
	std::cout << "[10] subida inicial por staging\n";
	auto device = make_device();
	{
		const std::vector<float> vertices(48, 2.0f);

		// Datos del mismo tamaño que el buffer.
		auto full = device->create_buffer(luxel::BufferDescriptor::from_data(vertices));
		CHECK(full->size() == vertices.size() * sizeof(float));
		CHECK(full->bindless_index() != 0);

		// Datos MAS PEQUEÑOS que el buffer: la copia debe ser del tamaño de los datos.
		auto desc = luxel::BufferDescriptor::from_data(vertices);
		desc.size = desc.initial_data.size() * 4;
		auto partial = device->create_buffer(desc);
		CHECK(partial->size() == desc.size);
		CHECK(partial->bindless_index() != 0);
		CHECK(partial->bindless_index() != full->bindless_index());

		// Index con datos: sin hueco bindless.
		const std::vector<uint32_t> indices{ 0, 1, 2, 2, 1, 3 };
		auto index = device->create_buffer(luxel::BufferDescriptor::from_data(indices, luxel::BufferUsage::Index));
		CHECK(index->bindless_index() == 0);

		// Storage | Index con datos.
		auto both = device->create_buffer(luxel::BufferDescriptor::from_data(indices, luxel::BufferUsage::Storage | luxel::BufferUsage::Index));
		CHECK(both->bindless_index() != 0);
	}
	CHECK(device->ref_count() == 1);
}

// 11. VulkanFrameAllocator aislado: offsets crecientes y alineados a 16, mismo
//     hueco bindless para todo el allocator, reset, desbordamiento y que el
//     destructor devuelve el hueco a la tabla.
static void test_frame_allocator()
{
	std::cout << "[11] VulkanFrameAllocator\n";
	auto device = make_device();
	auto& vk_device = *static_cast<luxel::vulkan::VulkanDevice*>(device.get());

	uint32_t first_slot = 0;
	{
		luxel::vulkan::VulkanFrameAllocator allocator(vk_device);
		const std::vector<std::byte> data(64, std::byte{ 0xAB });
		const std::span<const std::byte> bytes(data);

		auto a = allocator.allocate(bytes.first(16));
		auto b = allocator.allocate(bytes.first(20));
		auto c = allocator.allocate(bytes.first(4));
		auto d = allocator.allocate(bytes.first(16));

		CHECK(a.slot_index != 0);
		CHECK(a.slot_index == b.slot_index && b.slot_index == c.slot_index && c.slot_index == d.slot_index);
		CHECK(a.offset == 0);
		CHECK(b.offset == 16);   // 16 ya esta alineado
		CHECK(c.offset == 48);   // 16 + 20 = 36 -> 48
		CHECK(d.offset == 64);   // 48 + 4 = 52 -> 64
		first_slot = a.slot_index;

		// Reset: se vuelve a empezar desde 0 con el mismo hueco.
		allocator.reset_cursor();
		auto e = allocator.allocate(bytes.first(8));
		CHECK(e.offset == 0);
		CHECK(e.slot_index == first_slot);

		// Desbordamiento: justo la capacidad cabe, un byte mas no.
		const std::vector<std::byte> big(luxel::vulkan::k_frame_allocator_capacity, std::byte{ 0 });
		allocator.reset_cursor();
		auto full = allocator.allocate(big);
		CHECK(full.offset == 0);

		bool threw = false;
		try { allocator.allocate(bytes.first(1)); } catch (const std::runtime_error&) { threw = true; }
		CHECK(threw);

		allocator.reset_cursor();
		const std::vector<std::byte> too_big(luxel::vulkan::k_frame_allocator_capacity + 1, std::byte{ 0 });
		threw = false;
		try { allocator.allocate(too_big); } catch (const std::runtime_error&) { threw = true; }
		CHECK(threw);

		// Tras la excepcion el allocator sigue siendo usable.
		auto after = allocator.allocate(bytes.first(16));
		CHECK(after.offset == 0);
	}

	// El destructor ha devuelto el hueco: un allocator nuevo lo reutiliza.
	{
		luxel::vulkan::VulkanFrameAllocator allocator(vk_device);
		const std::vector<std::byte> data(16, std::byte{ 0 });
		CHECK(allocator.allocate(data).slot_index == first_slot);
	}

	CHECK(device->ref_count() == 1);
}

// 12. Frame::allocate dentro de frames reales: cada frame empieza en 0 (reset
//     en begin_frame), los dos frames en vuelo usan huecos distintos y las
//     asignaciones de un mismo frame no se solapan.
static void test_frame_allocate(vitreum::Window& window)
{
	std::cout << "[12] Frame::allocate en frames reales\n";
	FrameRig rig(window);
	const std::vector<std::byte> data(100, std::byte{ 0x7F });

	uint32_t slots[2]{};
	for (int i = 0; i < 6; ++i)
	{
		auto* frame = rig.begin();
		if (frame == nullptr) { continue; }

		auto a = frame->allocate(data);
		auto b = frame->allocate(data);
		CHECK(a.offset == 0);               // reset al empezar el frame
		CHECK(b.offset == 112);             // 100 -> 112
		CHECK(a.slot_index == b.slot_index);
		CHECK(a.slot_index != 0);
		slots[i % 2] = a.slot_index;

		frame->commands().begin_rendering({ .clear = { 0.0f, 0.0f, 0.0f, 1.0f } });
		frame->commands().end_rendering();
		rig.end(*frame);
	}
	CHECK(slots[0] != slots[1]);
}

int main()
{
	test_not_destroyed_immediately();
	test_device_destroyed_by_last_resource();

	vitreum::Window window;
	if (!window.create({ "test_deferred", { 640, 480 } })) {
		std::cout << "No se pudo crear la ventana\n";
		return 2;
	}

	test_released_between_frames(window);
	test_released_during_frame(window);

	test_descriptor_set_sampler_slots(window);
	test_pipeline();

	test_buffer_storage_slot(window);
	test_buffer_index_no_slot();
	test_buffer_invalid_descriptors();
	test_buffer_upload();

	test_frame_allocator();
	test_frame_allocate(window);

	std::cout << (g_failures == 0 ? "\nTODO OK\n" : "\nHAY FALLOS\n");
	return g_failures == 0 ? 0 : 1;
}

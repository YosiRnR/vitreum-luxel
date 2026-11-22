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
#include "backends/vulkan/vulkan_memory_allocator.h"
#include "backends/vulkan/vulkan_memory_block.h"

#include <filesystem>
#include <stdexcept>
#include <iostream>
#include <utility>
#include <cstdint>
#include <cstddef>
#include <cstring>
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

// 13. Sampler: tiene hueco bindless, distinto por sampler, y se devuelve de
//     forma diferida (igual que el buffer Storage del caso 7).
static void test_sampler_slot(vitreum::Window& window)
{
	std::cout << "[13] sampler: hueco bindless con liberacion diferida\n";
	FrameRig rig(window);

	auto s1 = rig.device->create_sampler({});
	auto s2 = rig.device->create_sampler({ .mag_filter = luxel::SamplerFilter::Nearest,
	                                       .min_filter = luxel::SamplerFilter::Nearest,
	                                       .address_mode_u = luxel::SamplerAddressMode::ClampToEdge });
	const uint32_t idx1 = s1->bindless_index();
	const uint32_t idx2 = s2->bindless_index();
	CHECK(idx1 != 0 && idx2 != 0);
	CHECK(idx1 != idx2);
	CHECK(s2->sampler_desc().mag_filter == luxel::SamplerFilter::Nearest);
	CHECK(s2->sampler_desc().address_mode_u == luxel::SamplerAddressMode::ClampToEdge);

	s1 = {};   // se encola; el hueco NO debe estar libre todavia
	auto s3 = rig.device->create_sampler({});
	const uint32_t idx3 = s3->bindless_index();
	CHECK(idx3 != 0 && idx3 != idx1 && idx3 != idx2);

	s3 = {};
	s2 = {};
	CHECK(rig.run_frame());
	CHECK(rig.run_frame());
	CHECK(rig.run_frame());   // ya han pasado los frames en vuelo: huecos libres

	auto s4 = rig.device->create_sampler({});
	const uint32_t idx4 = s4->bindless_index();
	CHECK(idx4 == idx1 || idx4 == idx2 || idx4 == idx3);   // reutiliza uno liberado
}

// 14. Soltar un sampler no deja al device retenido (la cola lo destruye al
//     soltar el device y no antes de tiempo).
static void test_sampler_releases_device()
{
	std::cout << "[14] sampler: sin retener el device\n";
	auto device = make_device();
	{
		auto sampler = device->create_sampler({});
		CHECK(sampler->bindless_index() != 0);
		CHECK(device->ref_count() == 2);   // nosotros + el sampler
	}
	CHECK(device->ref_count() == 1);
}

// Datos de una textura RGBA8 de w x h (todo a un valor; el contenido no se
// verifica aqui, solo la creacion).
static std::vector<std::byte> make_texture_data(uint32_t w, uint32_t h)
{
	return std::vector<std::byte>(static_cast<size_t>(w) * h * 4, std::byte{ 0x80 });
}

// 15. Textura: hueco bindless propio, liberacion diferida y reutilizacion
//     (igual que los casos 7 y 13). Solo comprueba la creacion (7c): el
//     contenido y el layout se veran cuando exista la subida (7d).
static void test_texture_slot(vitreum::Window& window)
{
	std::cout << "[15] textura: hueco bindless con liberacion diferida\n";
	FrameRig rig(window);

	const auto data = make_texture_data(4, 4);
	const luxel::TextureDescriptor desc{
		.width = 4, .height = 4,
		.format = luxel::Format::R8G8B8A8_SRGB,
		.initial_data = data
	};

	auto t1 = rig.device->create_texture(desc);
	CHECK(t1->width() == 4 && t1->height() == 4);
	CHECK(t1->format() == luxel::Format::R8G8B8A8_SRGB);
	CHECK(t1->mip_levels() == 1);
	CHECK(luxel::has_flag(t1->usage(), luxel::TextureUsage::Sampled));
	const uint32_t idx1 = t1->bindless_index();
	CHECK(idx1 != 0);

	auto t2 = rig.device->create_texture(desc);
	const uint32_t idx2 = t2->bindless_index();
	CHECK(idx2 != 0 && idx2 != idx1);

	t1 = {};   // se encola; el hueco NO debe estar libre todavia
	auto t3 = rig.device->create_texture(desc);
	const uint32_t idx3 = t3->bindless_index();
	CHECK(idx3 != 0 && idx3 != idx1 && idx3 != idx2);

	t2 = {};
	t3 = {};
	CHECK(rig.run_frame());
	CHECK(rig.run_frame());
	CHECK(rig.run_frame());   // ya han pasado los frames en vuelo

	auto t4 = rig.device->create_texture(desc);
	const uint32_t idx4 = t4->bindless_index();
	CHECK(idx4 == idx1 || idx4 == idx2 || idx4 == idx3);   // reutiliza uno liberado
}

// 16. create_texture rechaza descriptores invalidos y no deja el device retenido.
static void test_texture_invalid_descriptors()
{
	std::cout << "[16] create_texture rechaza descriptores invalidos\n";
	auto device = make_device();

	const auto data = make_texture_data(4, 4);
	const luxel::TextureDescriptor valid{
		.width = 4, .height = 4,
		.format = luxel::Format::R8G8B8A8_UNorm,
		.initial_data = data
	};

	auto throws = [&](luxel::TextureDescriptor desc) {
		try {
			device->create_texture(desc);
		}
		catch (const std::exception&) {
			return true;
		}
		return false;
	};

	auto d = valid; d.width = 0;                       CHECK(throws(d));   // ancho 0
	d = valid;      d.height = 0;                      CHECK(throws(d));   // alto 0
	d = valid;      d.format = luxel::Format::Undefined; CHECK(throws(d)); // formato sin tamaño
	d = valid;      d.usage = luxel::TextureUsage::None; CHECK(throws(d)); // sin uso
	d = valid;      d.width = 8;                       CHECK(throws(d));   // datos != 8*4*4
	d = valid;      d.initial_data = {};               CHECK(throws(d));   // sin datos (por ahora)
	d = valid;      d.mip_levels = 2;                  CHECK(throws(d));   // solo 1 mip por ahora

	CHECK(!throws(valid));
	CHECK(device->ref_count() == 1);   // ni los fallos ni la textura valida retienen el device
}

// 17. VulkanMemoryBlock aislado: reparto de offsets con alineacion, mismo
//     vk::DeviceMemory para todas las reservas, bloque lleno, reutilizacion y
//     puntero mapeado solo si el tipo es visible y coherente.
static void test_memory_block()
{
	std::cout << "[17] VulkanMemoryBlock\n";
	auto device = make_device();
	auto& vk_device = *static_cast<luxel::vulkan::VulkanDevice*>(device.get());

	using enum vk::MemoryPropertyFlagBits;
	constexpr uint64_t k_block_size = 1024 * 1024;
	const vk::MemoryPropertyFlags host_flags = eHostVisible | eHostCoherent;
	const auto mem_props = vk_device.physical_device().getMemoryProperties();

	// Bloque de tipo DeviceLocal. Puede ser visible desde la CPU (GPU integrada),
	// asi que el puntero mapeado debe existir si y solo si el tipo lo permite.
	{
		const uint32_t type = vk_device.find_memory_type(0xFFFFFFFFu, eDeviceLocal);
		const bool expect_mapped = (mem_props.memoryTypes[type].propertyFlags & host_flags) == host_flags;
		luxel::vulkan::VulkanMemoryBlock block(vk_device, k_block_size, type);

		CHECK(block.memory_type_index() == type);
		CHECK((block.mapped() != nullptr) == expect_mapped);

		auto a = block.allocate(256, 16);
		auto b = block.allocate(100, 64);
		auto c = block.allocate(100, 256);   // b termina en 356 -> sube a 512
		CHECK(a.has_value() && b.has_value() && c.has_value());
		if (!a || !b || !c) { return; }

		CHECK(a->offset == 0 && a->size == 256);
		CHECK(b->offset == 256 && b->size == 100);
		CHECK(c->offset == 512 && c->size == 100);
		CHECK(block.memory() != vk::DeviceMemory{});   // un unico vk::DeviceMemory para todas las reservas

		// Lleno: lo que queda libre es menos que el bloque entero.
		CHECK(!block.allocate(k_block_size, 1).has_value());

		block.free(b->offset, b->size);
		block.free(a->offset, a->size);
		block.free(c->offset, c->size);

		// Tras liberar todo, se reutiliza desde el principio.
		auto d = block.allocate(k_block_size, 1);
		CHECK(d.has_value() && d->offset == 0);
		if (d) { block.free(d->offset, d->size); }
	}

	// Bloque visible y coherente: puntero base, punteros = base + offset, y los
	// datos escritos por una reserva no pisan los de la otra.
	{
		const uint32_t type = vk_device.find_memory_type(0xFFFFFFFFu, host_flags);
		luxel::vulkan::VulkanMemoryBlock block(vk_device, k_block_size, type);
		CHECK(block.mapped() != nullptr);

		auto a = block.allocate(64, 16);
		auto b = block.allocate(64, 16);
		CHECK(a.has_value() && b.has_value());
		if (!a || !b) { return; }

		// El bloque ya no devuelve punteros: cada reserva es base + offset.
		std::byte* pa = block.mapped() + a->offset;
		std::byte* pb = block.mapped() + b->offset;
		CHECK(pa == block.mapped());
		CHECK(pb - pa == static_cast<std::ptrdiff_t>(b->offset - a->offset));

		std::memset(pa, 0xAA, 64);
		std::memset(pb, 0x55, 64);
		CHECK(pa[0] == std::byte{ 0xAA } && pa[63] == std::byte{ 0xAA });
		CHECK(pb[0] == std::byte{ 0x55 } && pb[63] == std::byte{ 0x55 });

		block.free(a->offset, a->size);
		block.free(b->offset, b->size);
	}

	CHECK(device->ref_count() == 1);
}

// 18. VulkanMemoryAllocator aislado: reparto dentro de un bloque, buffers e
//     imagenes en bloques distintos, bloque nuevo cuando se llena, recurso
//     mayor que el bloque, reutilizacion tras liberar (first-fit entre
//     bloques) y puntero mapeado en memoria visible.
static void test_memory_allocator()
{
	std::cout << "[18] VulkanMemoryAllocator\n";
	auto device = make_device();
	auto& vk_device = *static_cast<luxel::vulkan::VulkanDevice*>(device.get());

	using luxel::vulkan::ResourceKind;
	using enum vk::MemoryPropertyFlagBits;
	constexpr uint64_t k_block = luxel::vulkan::VulkanMemoryAllocator::k_default_block_size;
	auto req = [](uint64_t size, uint64_t alignment) {
		return vk::MemoryRequirements{ .size = size, .alignment = alignment, .memoryTypeBits = 0xFFFFFFFFu };
	};

	const uint32_t type = vk_device.find_memory_type(0xFFFFFFFFu, eDeviceLocal);
	luxel::vulkan::VulkanMemoryAllocator allocator(vk_device);
	CHECK(allocator.block_count() == 0);

	// Dos reservas pequenas comparten el primer bloque.
	auto a = allocator.allocate(req(256, 16), eDeviceLocal, ResourceKind::Buffer);
	auto b = allocator.allocate(req(256, 16), eDeviceLocal, ResourceKind::Buffer);
	CHECK(allocator.block_count() == 1);
	CHECK(a.offset == 0 && b.offset == 256);
	CHECK(a.memory == b.memory && a.block == b.block);

	// Una imagen del mismo tipo de memoria va a otro bloque (granularidad).
	auto img = allocator.allocate(req(256, 16), eDeviceLocal, ResourceKind::Image);
	CHECK(allocator.block_count() == 2);
	CHECK(allocator.block_count(ResourceKind::Buffer, type) == 1);
	CHECK(allocator.block_count(ResourceKind::Image, type) == 1);
	CHECK(img.memory != a.memory);

	// Rellenamos el resto del primer bloque exactamente: sigue siendo uno.
	auto c = allocator.allocate(req(k_block - 512, 16), eDeviceLocal, ResourceKind::Buffer);
	CHECK(c.offset == 512 && c.memory == a.memory);
	CHECK(allocator.block_count(ResourceKind::Buffer, type) == 1);

	// Ya no cabe nada mas: aparece un segundo bloque de buffers.
	auto d = allocator.allocate(req(16, 16), eDeviceLocal, ResourceKind::Buffer);
	CHECK(allocator.block_count(ResourceKind::Buffer, type) == 2);
	CHECK(d.memory != a.memory && d.offset == 0);

	// Un recurso mayor que el bloque por defecto obtiene un bloque a su medida.
	auto big = allocator.allocate(req(2 * k_block, 16), eDeviceLocal, ResourceKind::Buffer);
	CHECK(allocator.block_count(ResourceKind::Buffer, type) == 3);
	CHECK(big.offset == 0 && big.size == 2 * k_block);
	CHECK(big.memory != a.memory && big.memory != d.memory);

	// Tras liberar el principio del primer bloque, se reutiliza (first-fit
	// entre bloques: el primero con hueco) y no se crea otro.
	allocator.free(a);
	allocator.free(b);
	auto e = allocator.allocate(req(256, 16), eDeviceLocal, ResourceKind::Buffer);
	CHECK(e.memory == a.memory && e.offset == 0);
	CHECK(allocator.block_count(ResourceKind::Buffer, type) == 3);

	// Memoria visible y coherente: puntero mapeado = base del bloque + offset.
	const vk::MemoryPropertyFlags host_flags = eHostVisible | eHostCoherent;
	auto h1 = allocator.allocate(req(64, 16), host_flags, ResourceKind::Buffer);
	auto h2 = allocator.allocate(req(64, 16), host_flags, ResourceKind::Buffer);
	CHECK(h1.mapped != nullptr && h2.mapped != nullptr);
	CHECK(h2.mapped - h1.mapped == static_cast<std::ptrdiff_t>(h2.offset - h1.offset));
	std::memset(h1.mapped, 0x11, 64);
	std::memset(h2.mapped, 0x22, 64);
	CHECK(h1.mapped[63] == std::byte{ 0x11 } && h2.mapped[0] == std::byte{ 0x22 });

	// Se libera todo antes de que mueran los bloques (assert del destructor).
	for (const auto& allocation : { e, c, d, big, img, h1, h2 }) {
		allocator.free(allocation);
	}

	CHECK(device->ref_count() == 1);
}

// 19. El VulkanDevice tiene su asignador de memoria: es utilizable y el device
//     se destruye sin problemas (los bloques mueren antes que el vk::Device y
//     sin asignaciones vivas, si no saltaria el assert del bloque).
static void test_device_memory_allocator()
{
	std::cout << "[19] VulkanDevice::memory_allocator\n";
	auto device = make_device();
	auto& vk_device = *static_cast<luxel::vulkan::VulkanDevice*>(device.get());
	auto& allocator = vk_device.memory_allocator();

	CHECK(allocator.block_count() == 0);

	const vk::MemoryRequirements req{ .size = 1024, .alignment = 16, .memoryTypeBits = 0xFFFFFFFFu };
	auto a = allocator.allocate(req, vk::MemoryPropertyFlagBits::eDeviceLocal, luxel::vulkan::ResourceKind::Buffer);
	CHECK(static_cast<bool>(a));   // operator bool es explicit: no convierte solo al pasarlo a check()
	CHECK(allocator.block_count() == 1);
	allocator.free(a);

	// Sin asignaciones vivas el device se destruye al soltar el ultimo Ref.
	CHECK(device->ref_count() == 1);
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

	test_sampler_slot(window);
	test_sampler_releases_device();

	test_texture_slot(window);
	test_texture_invalid_descriptors();

	test_memory_block();
	test_memory_allocator();
	test_device_memory_allocator();

	std::cout << (g_failures == 0 ? "\nTODO OK\n" : "\nHAY FALLOS\n");
	return g_failures == 0 ? 0 : 1;
}

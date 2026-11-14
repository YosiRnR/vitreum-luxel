// Pruebas de la destruccion diferida (hito 4).
//
// Es un test de "caja blanca": incluye cabeceras privadas de luxel (ver el
// CMakeLists de esta carpeta) para poder encolar un recurso en el VulkanDevice.
// Cuando existan Buffer/Texture reales, esta clase TestResource desaparece:
// los recursos reales harian exactamente lo mismo en su on_zero_refs().

#include <vitreum/vitreum.h>
#include <luxel/luxel.h>
#include "backends/vulkan/vulkan_device.h"

#include <iostream>
#include <utility>
#include <cstdint>

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

	std::cout << (g_failures == 0 ? "\nTODO OK\n" : "\nHAY FALLOS\n");
	return g_failures == 0 ? 0 : 1;
}

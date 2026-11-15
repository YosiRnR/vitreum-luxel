#pragma once
#include <luxel/types.h>
#include <string_view>
#include <cstdint>
#include <span>

namespace luxel
{
	struct GraphicsPipelineDescriptor
	{
		// El array de bytes que cree el usuario solo se va a utilizar durante la creación, salvo que el usuario crease el array, luego crease el Descriptor este pero lo usase fuera del ámbito de dónde creo el array, entonces sería un problema
		std::span<const uint32_t> vs_byte_code;
		std::span<const uint32_t> ps_byte_code;

		Format format{ Format::Undefined };
		PrimitiveTopology topology{ PrimitiveTopology::TriangleList };

		// No sé si compensa string_view o directamente string por valor...con string_view necesito hacer copia local a string para garantizar que la cadena sea null-terminated que es lo que espera luego el pName de Vulkan
		std::string_view vs_entry_name{ "vs_main" };
		std::string_view ps_entry_name{ "ps_main" };
	};

	class GraphicsPipeline : public RefCounted
	{
	public:
		virtual ~GraphicsPipeline() = default;
	};

} // namespace luxel

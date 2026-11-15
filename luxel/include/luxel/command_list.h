#pragma once

#include <cstdint>

namespace luxel
{
	struct ClearColor
	{
		float r;
		float g;
		float b;
		float a;
	};

	struct RenderingDescriptor
	{
		ClearColor clear;
	};

	class GraphicsPipeline;

	class CommandList
	{
	public:
		virtual ~CommandList() = default;

		virtual void begin_rendering(const RenderingDescriptor&) = 0;
		virtual void end_rendering() = 0;

		virtual void bind_pipeline(GraphicsPipeline&) = 0;
		virtual void draw(uint32_t num_vertices) = 0;
	};

} // namespace luxel

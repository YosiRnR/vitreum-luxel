#pragma once

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

	class CommandList
	{
	public:
		virtual ~CommandList() = default;

		virtual void begin_rendering(const RenderingDescriptor&) = 0;
		virtual void end_rendering() = 0;
	};

} // namespace luxel

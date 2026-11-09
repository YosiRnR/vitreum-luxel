#pragma once
#include <luxel/command_list.h>

namespace luxel
{
	class Frame
	{
	public:
		virtual ~Frame() = default;

		virtual CommandList& commands() = 0;
	};

} // namespace luxel

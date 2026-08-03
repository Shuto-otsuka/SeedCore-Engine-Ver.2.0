#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	enum class BlendStateType
	{
		Opaque,
		Alpha,
		Additive,
		Multiply,
		Subtractive,
		Screen,
	};

	class BlendState
	{
	public:
		static void Create();

		static D3D12_BLEND_DESC Get(BlendStateType type);
	};
}

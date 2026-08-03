#pragma once
#include <d3d12.h>

namespace SeedCore
{
	enum class D3D12CommandType
	{
		Direct,
		Compute,
		Copy,
	};

	inline D3D12_COMMAND_LIST_TYPE ToDxCmdType(D3D12CommandType type)
	{
		switch (type)
		{
		case D3D12CommandType::Compute:
			return D3D12_COMMAND_LIST_TYPE_COMPUTE;
		case D3D12CommandType::Copy:
			return D3D12_COMMAND_LIST_TYPE_COPY;
		case D3D12CommandType::Direct:
			[[fallthrough]];
		default:
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
		}
	}
}
#include <GraphicsEngine/Sky/Skymap.h>

namespace SeedCore
{
	Uint Skymap::ShaderResourceViewIndex()const
	{
		return shaderResourceViewIndex_;
	}

	Bool Skymap::Valid()const
	{
		return shaderResourceViewIndex_ != SC_INVALID;
	}

	Handle<Skymap> Skymap::GetHandle()const
	{
		return handle_;
	}
}

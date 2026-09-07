#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/String.h>
#include <GraphicsEngine/Model/ModelLoader.h>

namespace SeedCore
{
	class Crister;

	class ModelExporter :public NonCopyable
	{
	public:
		ModelExporter() = default;
		~ModelExporter() = default;

		Bool Export(const Crister& crister, ModelFormat format, String filePath);
	};
}

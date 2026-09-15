#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/String.h>
#include <GraphicsEngine/Model/ModelLoader.h>

namespace SeedCore
{
	class Crister;

	enum class ExportAxis
	{
		GltfRightHandedYUp,
		MayaYUp,
		UnrealZUpLeftHanded,
		EngineNativeDirectX,
	};

	enum class ExportPreset
	{
		Gltf,
		Glb,
		FbxMaya,
		FbxUnreal,
		FbxUnity,
		FbxNative,
	};

	struct ExportProfile
	{
		ModelFormat format_ = ModelFormat::Gltf;
		Bool binary_ = false;
		ExportAxis axis_ = ExportAxis::GltfRightHandedYUp;
		Float unitScale_ = 1.0f;
	};

	class ModelExporter :public NonCopyable
	{
	public:
		ModelExporter() = default;
		~ModelExporter() = default;

		[[nodiscard]] static ExportProfile Preset(ExportPreset preset);

		Bool Export(const Crister& crister, const ExportProfile& profile, String filePath);

		Bool Export(const tinygltf::Model& model, const ExportProfile& profile, String filePath);
	};
}

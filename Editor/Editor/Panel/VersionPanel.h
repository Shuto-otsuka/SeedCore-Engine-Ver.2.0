#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;

	class VersionPanel
	{
	public:
		VersionPanel(EditorContext& context);
		~VersionPanel() = default;

		void Draw();

		void Open();

	private:
		Bool show_ = false;

		ImTextureID logoTextureId_ = 0;
		Microsoft::WRL::ComPtr<ID3D12Resource> logoResource_;

		Float logoWidth_ = 0.0f;
		Float logoHeight_ = 0.0f;

		String gpuName_;
		String osInfo_;
		String memoryInfo_;
	};
}

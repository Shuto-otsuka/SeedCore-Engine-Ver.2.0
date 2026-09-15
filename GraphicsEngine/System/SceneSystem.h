#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>

namespace SeedCore
{
	struct SceneConstantBuffer
	{
		Matrix view_;
		Matrix inverseView_;

		Matrix projection_;
		Matrix inverseProjection_;
		Matrix nonJitterProjection_;

		Matrix currentViewProjection_;
		Matrix previousViewProjection_;
		Matrix inverseViewProjection_;
		Matrix nonJitterViewProjection_;

		Matrix previousNonJitterViewProjection_;

		Vector4 cameraPosition_;
		Vector4 cameraFocus_;

		Float fieldOfView_;
		Float nearPlane_;
		Float farPlane_;
		Uint viewMode_;

		Float totalTime_;
		Float deltaTime_;

		/// [EN] Resolution of whatever the post-tonemap debug overlay (collider wireframes, selection outline) actually draws onto - screenSize_ normally, or PostProcessRenderer's DLSS-RR-upscaled output resolution while DLSS-RR is active. OutlinePS.hlsl scales SV_Position by screen_size_/display_size_ before indexing the (native-resolution) silhouette.
		/// [JP] トーンマップ後デバッグオーバーレイ(コライダーワイヤーフレーム、選択アウトライン)が実際に描画する先の解像度 - 通常はscreenSize_、DLSS-RR有効時はPostProcessRendererのDLSS-RRアップスケール後出力解像度。OutlinePS.hlslは(ネイティブ解像度の)シルエットをインデックスする前にSV_Positionをscreen_size_/display_size_でスケールする。
		Vector2 displaySize_;

		Vector2 screenSize_;
		Vector2 inverseScreenSize_;
	};

	class BindlessHeap;

	class SEEDCORE_API SceneSystem
	{
	public:
		SceneSystem(ID3D12Device* device, BindlessHeap* bindlessHeap);
		~SceneSystem();

		void Upload(SceneConstantBuffer buffer);

		Uint GetIndex()const;

	private:
		ResourcePtr<ConstantBuffer<SceneConstantBuffer>> sceneConstantBuffer_;

	};
}
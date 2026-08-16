#include "../Shader/Constants.hlsli"
#include "../Shader/Structured.hlsli"

/**
* [EN]
* Patches G-Buffer RT3 (velocity) for background pixels (sky/procedural
* clouds) with a proper camera-rotation-derived motion vector. Rasterized
* opaque geometry (StaticModelMS.hlsl etc.) already writes correct velocity
* into RT3; background pixels are never touched by that pass, so RT3 stays
* at its per-frame clear value (0,0) there. Since Model/Opaque/DeferredLightingPS.hlsl
* draws the sky/clouds as a screen-space effect with no G-Buffer contribution
* of its own, DLSS Ray Reconstruction previously saw "zero motion" for the
* entire sky/cloud region even while panning the camera, causing severe
* ghosting there (clouds/sky occupy a large fraction of the screen, so the
* error was highly visible). This pass reconstructs each background pixel's
* view ray, projects a point far along that ray through both the current and
* previous (both jittered, matching StaticModelMS.hlsl's convention)
* view-projection matrices, and writes the resulting NDC delta - at that
* distance camera translation is negligible, so this approximates
* rotation-only reprojection, appropriate for an effectively infinitely
* distant sky.
*
* Only DlssRayReconstructionRenderer runs this, and only when DLSS-RR is
* active - the engine's own Shadow/AO/GI denoisers don't read velocity for
* background pixels at all (they early-out on depth == 0 before ever
* touching it).
*
* [JP]
* G-Buffer RT3(velocity)の背景ピクセル(空/プロシージャル雲)を、カメラ回転
* から導いた正しい速度でパッチする。ラスタ化される不透明ジオメトリ
* (StaticModelMS.hlsl 等)は既に正しい速度をRT3へ書いているが、背景ピクセルは
* そのパスが一切触れないため、毎フレームのクリア値(0,0)のまま残る。
* Model/Opaque/DeferredLightingPS.hlsl は空/雲をG-Bufferに寄与しないスクリーン
* スペース効果として描くため、DLSS Ray Reconstruction はカメラをパンしても
* 空/雲の領域全体を「速度ゼロ」と見なし、そこで激しいゴーストが出ていた
* (空/雲は画面の大部分を占めるため誤差が非常に目立つ)。このパスは各背景
* ピクセルの視線レイを復元し、そのレイの十分遠い点を現在と前フレーム
* (どちらもジッタ有り、StaticModelMS.hlsl と同じ規約)の view-projection
* 行列で射影して、そのNDC差分を書く — その距離ではカメラの並進はほぼ無視
* できるため、実質的に回転のみのリプロジェクションを近似する。これは
* 事実上無限遠にある空に対して妥当な近似。
*
* このパスを実行するのは DlssRayReconstructionRenderer のみ、しかも
* DLSS-RR が有効な間だけ — エンジン自前の Shadow/AO/GI デノイザは背景
* ピクセルの速度を一切読まない(depth == 0 で早期リターンし、そこに触れる
* 前に抜ける)。
*/
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	uint2 pixel = dtid.xy;

	// DeferredLightingPS.hlsl と同じ「背景」判定(法線バッファが全ゼロ)。
	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float4 rt1 = normal_texture.Load(int3(pixel, 0));
	if (dot(rt1, rt1) != 0.0)
	{
		return;
	}

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
	float4 clip = float4(ndc, depth, 1.0);
	float4 world = mul(clip, scene.inverse_view_projection_);
	float3 view_direction = normalize(world.xyz / world.w - scene.camera_position_.xyz);

	// [JP] 空は実質無限遠。この距離まで進めた点を current/previous の
	//      view-projection で射影すれば、並進の影響は無視できるほど小さくなり
	//      回転のみのリプロジェクションを近似できる。
	const float PSEUDO_INFINITE_DISTANCE = 100000.0;
	float3 far_world_position = scene.camera_position_.xyz + view_direction * PSEUDO_INFINITE_DISTANCE;

	// [JP] StaticModelMS.hlsl と同じ規約(current_view_projection_/
	//      previous_view_projection_、どちらもジッタ有り)で計算する。
	float4 current_clip = mul(float4(far_world_position, 1.0), scene.current_view_projection_);
	float4 previous_clip = mul(float4(far_world_position, 1.0), scene.previous_view_projection_);

	float2 current_ndc = current_clip.xy / current_clip.w;
	float2 previous_ndc = previous_clip.xy / previous_clip.w;
	float2 velocity = (current_ndc - previous_ndc) * 0.5;

	RWTexture2D<float2> velocity_output = ResourceDescriptorHeap[structured_indices.gbuffer_.velocity_uav_index_];
	velocity_output[pixel] = velocity;
}

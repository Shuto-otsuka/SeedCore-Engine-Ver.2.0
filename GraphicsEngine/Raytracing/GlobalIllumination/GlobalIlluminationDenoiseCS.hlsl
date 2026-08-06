#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Denoiser.hlsli"

static const float GI_TEMPORAL_BLEND_ALPHA = 0.05;

// [JP] 分散クリッピングの許容幅(近傍の標準偏差の何倍まで履歴を許すか)。
//      小さいほど残像(ゴースト)を抑えるがノイズが残り、大きいほど滑らかに
//      なるがゴーストしやすい。GI は元信号のノイズが大きいので TAA の典型値
//      (1.0 前後)を採る。
static const float GI_HISTORY_CLIP_GAMMA = 1.0;

// [JP] DenoiserSpatialWeight の深度/法線の鋭さ。5x5 空間フィルタと 3x3
//      モーメント集計の両方で共用する(元実装と同じ値)。
static const float GI_DEPTH_SHARPNESS = 48.0;
static const float GI_NORMAL_POWER = 8.0;

/**
* [EN]
* Spatio-temporal denoiser for the raw 1spp stochastic GI radiance
* (GlobalIlluminationRT.hlsl's output). First a 5x5 depth/normal-weighted
* bilateral average smooths the per-pixel hemisphere-sample noise spatially
* (background/invalid neighbors, raw.a == 0, are excluded from the average so
* geometry edges don't pick up black from the sky); a 3x3 weighted color
* moment (mean/variance) is gathered at the same time to build a variance
* clipping box. Then the previous frame's accumulated radiance is
* reprojected via the G-Buffer velocity buffer, clamped to the variance box
* (rejects ghosting from disocclusion/camera cuts/the uninitialized first
* frame) and exponentially blended toward the spatially filtered sample.
* Same overall structure as AmbientOcclusionDenoiseCS.hlsl, extended from a
* scalar 0/1 signal to continuous HDR RGB (so a min/max-only clamp would be
* far too loose - variance clipping is used instead).
*
* The result is written to the A-Trous scratch0 texture, NOT the final
* accumulated/history slot - ATrousPass1/2/3 below run after this (see
* GlobalIlluminationRenderer::Dispatch) and further spatially filter it
* before it becomes this frame's composited result and next frame's history.
*
* [JP]
* 生の 1spp 確率的GI放射輝度(GlobalIlluminationRT.hlsl の出力)の空間+時間
* デノイザ。まず 5x5 の深度/法線重み付きバイラテラル平均でピクセルごとの
* 半球サンプルノイズを空間的に均す(背景/無効な近傍 raw.a == 0 は平均から
* 除外するので、ジオメトリの縁が空の黒を拾わない)。同じループ内で 3x3 の
* 重み付きカラーモーメント(平均/分散)も集計し、分散クリッピングの箱を作る。
* 続けて前フレームの蓄積放射輝度を G-Buffer の速度バッファでリプロジェクション
* し、分散の箱にクランプ(ディスオクルージョン・カメラカット・未初期化初回
* フレームのゴーストを棄却)した上で、空間フィルタ済みサンプルへ指数ブレンド
* する。AmbientOcclusionDenoiseCS.hlsl と同じ全体構成だが、二値0/1信号から
* 連続値のHDR RGBへ拡張しているため(min/maxだけのクランプでは緩すぎる)、
* 代わりに分散クリッピングを使う。
*
* 結果は最終的な蓄積/履歴スロットではなく A-Trous スクラッチ0テクスチャへ
* 書く - この後(GlobalIlluminationRenderer::Dispatch 参照)下の
* ATrousPass1/2/3 が続けて実行され、今フレームの合成結果・次フレームの履歴に
* なる前にさらに空間的にフィルタする。
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

	// raw はビュー共有(structured_indices)、蓄積チェーンはビューごと
	// (constant_indices)から取る。
	Texture2D<float4> raw_radiance = ResourceDescriptorHeap[structured_indices.global_illumination_.output_srv_index_];
	RWTexture2D<float4> scratch_output = ResourceDescriptorHeap[constant_indices.global_illumination_.atrous_scratch0_uav_index_];

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		scratch_output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;

	// velocity は (current_ndc - previous_ndc) * 0.5 で書き込まれている
	// (StaticModelPS.hlsl 等)。NDC->UV 変換で y は反転するため、UV 空間の
	// 移動量は (velocity.x, -velocity.y)。
	Texture2D<float2> velocity_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_2_];
	float2 velocity = velocity_texture.Load(int3(pixel, 0));
	float2 delta_uv = float2(velocity.x, -velocity.y);
	float2 previous_uv = uv - delta_uv;

	// [JP] 空間フィルタ: 5x5 のバイラテラル平均で 1spp のノイズを均してから
	//      時間積分する。重みは AmbientOcclusionDenoiseCS.hlsl と同じ
	//      「平面フィット深度」+「法線の一致度」に加え、近傍自身が背景
	//      (raw.a == 0)なら重み0にして、ジオメトリの縁が空の黒を拾わない
	//      ようにする。
	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float3 center_normal = OctNormalDecode(normal_texture.Load(int3(pixel, 0)).rg);

	int2 screen_max = int2(scene.screen_size_) - 1;

	// 局所深度勾配(中心差分)。同一平面上の傾き成分を表す。
	float2 depth_gradient = DenoiserDepthGradient(depth_texture, int2(pixel), screen_max);

	float weight_sum = 0.0;
	float3 filtered_raw = float3(0, 0, 0);

	// [JP] 分散クリッピング用のモーメント統計と近傍min/maxは中心3x3のみで
	//      集計する(AmbientOcclusionDenoiseCS.hlsl と同じ、広げすぎると
	//      履歴棄却が甘くなるため)。
	DenoiserMoments moments = DenoiserMomentsInit();

	[unroll]
	for (int dy = -2; dy <= 2; ++dy)
	{
		[unroll]
		for (int dx = -2; dx <= 2; ++dx)
		{
			int2 neighbor = clamp(int2(pixel) + int2(dx, dy), int2(0, 0), screen_max);
			float4 neighbor_raw = raw_radiance.Load(int3(neighbor, 0));
			float neighbor_depth = depth_texture.Load(int3(neighbor, 0));
			float3 neighbor_normal = OctNormalDecode(normal_texture.Load(int3(neighbor, 0)).rg);

			float spatial_weight = DenoiserSpatialWeight(depth, depth_gradient, int2(dx, dy), neighbor_depth, center_normal, neighbor_normal, GI_DEPTH_SHARPNESS, GI_NORMAL_POWER);

			// 背景/無効な近傍(GlobalIlluminationRayGeneration が a=0 で書いた
			// 画素)は完全に除外する。
			float weight = spatial_weight * neighbor_raw.a;

			filtered_raw += neighbor_raw.rgb * weight;
			weight_sum += weight;

			if (abs(dx) <= 1 && abs(dy) <= 1)
			{
				DenoiserMomentsAccumulate(moments, neighbor_raw.rgb, weight);
			}
		}
	}

	if (weight_sum > 0.0001)
	{
		filtered_raw /= weight_sum;
	}
	else
	{
		// [JP] 有効な近傍が一切無い(孤立ピクセル): 生の自分自身を使う。
		filtered_raw = raw_radiance.Load(int3(pixel, 0)).rgb;
	}

	// [JP] AmbientOcclusionDenoiseCS.hlsl と同じく、分散ベースの箱と
	//      3x3生値min/maxの箱の【交差】を取る(filtered_rawを含むまで
	//      min/max側を広げてから交差させ、下限が上限を追い越すのを防ぐ)。
	float3 clip_min = DenoiserVarianceClipMin(moments, filtered_raw, GI_HISTORY_CLIP_GAMMA);
	float3 clip_max = DenoiserVarianceClipMax(moments, filtered_raw, GI_HISTORY_CLIP_GAMMA);

	Texture2D<float4> history_radiance = ResourceDescriptorHeap[constant_indices.global_illumination_.history_srv_index_];
	float3 result = DenoiserTemporalBlend(history_radiance, previous_uv, clip_min, clip_max, filtered_raw, GI_TEMPORAL_BLEND_ALPHA);

	scratch_output[pixel] = float4(result, 1.0);
}

// [JP] ATrousPass1/2/3 共通のディスパッチ本体。source/dest はエントリポイント
//      ごとに固定(呼び出し元 GlobalIlluminationRenderer::Dispatch がビュー
//      ごとに3回、正しい順序でディスパッチする)。深度/法線は main() と同じ
//      G-Buffer から読む。無効画素(背景)は a=0 のまま素通しする。
void AtrousPassCommon(uint2 pixel, Texture2D<float4> source, RWTexture2D<float4> dest, int step)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		dest[pixel] = float4(0, 0, 0, 0);
		return;
	}

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float3 center_normal = OctNormalDecode(normal_texture.Load(int3(pixel, 0)).rg);

	int2 screen_max = int2(scene.screen_size_) - 1;
	float2 depth_gradient = DenoiserDepthGradient(depth_texture, int2(pixel), screen_max);

	float3 filtered = DenoiserATrousPass(source, depth_texture, normal_texture, int2(pixel), screen_max, depth, depth_gradient, center_normal, step, GI_DEPTH_SHARPNESS, GI_NORMAL_POWER);

	dest[pixel] = float4(filtered, 1.0);
}

/**
* [EN]
* A-Trous wavelet passes (Dammertz et al. 2010) further spatially denoising
* main()'s temporally-blended result, 3 separate compute-shader entry points
* compiled from this same file (see GlobalIlluminationDenoiseShader::Create -
* ShaderCache::GetOrCreateComputeShader takes a distinct entry point name per
* PSO). GlobalIlluminationRenderer::Dispatch runs all 3 in order, once per
* view per frame, ping-ponging between the two scratch textures with a
* doubling step (1, 2, 4) - the last pass writes directly into this frame's
* accumulated/history slot, so what DeferredLightingPS.hlsl samples this
* frame and what next frame's temporal blend reprojects as history is the
* A-Trous-filtered result, not just the temporally-blended one. See
* Denoiser.hlsli's DenoiserATrousPass for why a doubling step across several
* passes is cheaper than one wide single-pass blur.
*
* [JP]
* a-trous ウェーブレットパス(Dammertz et al. 2010)。main() が時間的に
* ブレンドした結果をさらに空間的にデノイズする、同じファイルからコンパイルする
* 3つの独立したコンピュートシェーダ・エントリポイント
* (GlobalIlluminationDenoiseShader::Create 参照 — ShaderCache::
* GetOrCreateComputeShader は PSO ごとに別のエントリポイント名を取れる)。
* GlobalIlluminationRenderer::Dispatch がビューごとに毎フレーム3つ順番に
* 実行し、2枚のスクラッチテクスチャ間を step を倍々(1, 2, 4)にしながら
* ピンポンする - 最後のパスは今フレームの蓄積/履歴スロットへ直接書くので、
* DeferredLightingPS.hlsl が今フレーム読むのも、次フレームの時間的ブレンドが
* 履歴としてリプロジェクションするのも、単なる時間的ブレンド結果ではなく
* A-Trousフィルタ済みの結果になる。step を倍々にした複数パスが1回の広い
* シングルパスぼかしより安く済む理由は Denoiser.hlsli の DenoiserATrousPass
* 参照。
*/
[numthreads(8, 8, 1)]
void ATrousPass1(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();
	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.global_illumination_.atrous_scratch0_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.global_illumination_.atrous_scratch1_uav_index_];
	AtrousPassCommon(dtid.xy, source, dest, 1);
}

[numthreads(8, 8, 1)]
void ATrousPass2(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();
	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.global_illumination_.atrous_scratch1_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.global_illumination_.atrous_scratch0_uav_index_];
	AtrousPassCommon(dtid.xy, source, dest, 2);
}

// [JP] 最終パスだけ dest が本物の蓄積/履歴スロット(ビューごとに毎フレーム
//      ピンポンする accumulated_uav_index_)を指す — スクラッチ同士では
//      なくなる点に注意。
[numthreads(8, 8, 1)]
void ATrousPass3(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();
	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.global_illumination_.atrous_scratch0_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.global_illumination_.accumulated_uav_index_];
	AtrousPassCommon(dtid.xy, source, dest, 4);
}

#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Denoiser.hlsli"

static const float GI_TEMPORAL_BLEND_ALPHA = 0.05;

/// [EN] Variance-clipping tolerance (how many neighborhood standard
///      deviations history is allowed to deviate by). Smaller suppresses
///      ghosting but leaves more noise; larger smooths more but ghosts more
///      easily. GI's raw signal is noisier than most, so this takes TAA's
///      typical value (around 1.0).
/// [JP] 分散クリッピングの許容幅(近傍の標準偏差の何倍まで履歴を許すか)。
///      小さいほど残像(ゴースト)を抑えるがノイズが残り、大きいほど滑らかに
///      なるがゴーストしやすい。GI は元信号のノイズが大きいので TAA の
///      典型値(1.0 前後)を採る。
static const float GI_HISTORY_CLIP_GAMMA = 1.0;

/// [EN] Depth/normal sharpness for DenoiserSpatialWeight - shared by both the
///      5x5 spatial filter and the 3x3 moment gathering (same values as the
///      original implementation).
/// [JP] DenoiserSpatialWeight の深度/法線の鋭さ。5x5 空間フィルタと 3x3
///      モーメント集計の両方で共用する(元実装と同じ値)。
static const float GI_DEPTH_SHARPNESS = 48.0;
static const float GI_NORMAL_POWER = 8.0;

/**
* [EN]
* Reference:
* - https://research.nvidia.com/publication/2017-07_spatiotemporal-variance-guided-filtering-real-time-reconstruction-path-traced
*   (Schied et al., "Spatiotemporal Variance-Guided Filtering", HPG 2017 -
*   the temporal variance-clipping scheme this pass's main() implements.)
* - https://jo.dreggn.org/home/2010_atrous.pdf
*   (Dammertz et al., "Edge-Avoiding A-Trous Wavelet Transform for
*   Fast Global Illumination Filtering", HPG 2010 - the wavelet passes below
*   main().)
*
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

	/// [EN] Bounds guard: the dispatch is rounded up to a multiple of the
	///      8x8 thread group size, so threads past the actual screen edge
	///      must bail out before touching any resource.
	/// [JP] 範囲外ガード: ディスパッチは 8x8 スレッドグループの倍数に
	///      切り上げられているので、実際の画面端を超えたスレッドはどの
	///      リソースにも触れる前に抜ける必要がある。
	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	uint2 pixel = dtid.xy;

	/// [EN] raw is view-shared (structured_indices); the accumulation chain
	///      is per-view (constant_indices).
	/// [JP] raw はビュー共有(structured_indices)、蓄積チェーンはビューごと
	///      (constant_indices)から取る。
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

	/// [EN] Velocity is written as (current_ndc - previous_ndc) * 0.5 (see
	///      StaticModelPS.hlsl etc.). NDC->UV conversion flips y, so the
	///      UV-space displacement is (velocity.x, -velocity.y).
	/// [JP] velocity は (current_ndc - previous_ndc) * 0.5 で書き込まれて
	///      いる(StaticModelPS.hlsl 等)。NDC->UV 変換で y は反転するため、
	///      UV 空間の移動量は (velocity.x, -velocity.y)。
	Texture2D<float2> velocity_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_2_];
	float2 velocity = velocity_texture.Load(int3(pixel, 0));
	float2 delta_uv = float2(velocity.x, -velocity.y);
	float2 previous_uv = uv - delta_uv;

	/// [EN] Spatial filter: smooth the 1spp noise with a 5x5 bilateral
	///      average before temporal integration. The weight is the same
	///      "plane-fit depth" + "normal agreement" as
	///      AmbientOcclusionDenoiseCS.hlsl, plus a zero weight when the
	///      neighbor itself is background (raw.a == 0), so a geometry edge
	///      doesn't pick up the sky's black.
	/// [JP] 空間フィルタ: 5x5 のバイラテラル平均で 1spp のノイズを均してから
	///      時間積分する。重みは AmbientOcclusionDenoiseCS.hlsl と同じ
	///      「平面フィット深度」+「法線の一致度」に加え、近傍自身が背景
	///      (raw.a == 0)なら重み0にして、ジオメトリの縁が空の黒を拾わない
	///      ようにする。
	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float3 center_normal = OctNormalDecode(normal_texture.Load(int3(pixel, 0)).rg);

	int2 screen_max = int2(scene.screen_size_) - 1;

	/// [EN] Local depth gradient (central difference) - represents the
	///      surface's tilt if it is a single plane.
	/// [JP] 局所深度勾配(中心差分)。同一平面上の傾き成分を表す。
	float2 depth_gradient = DenoiserDepthGradient(depth_texture, int2(pixel), screen_max);

	float weight_sum = 0.0;
	float3 filtered_raw = float3(0, 0, 0);

	/// [EN] The variance-clipping moment statistics are gathered from the
	///      center 3x3 only (same as AmbientOcclusionDenoiseCS.hlsl -
	///      widening it makes history rejection too lax).
	/// [JP] 分散クリッピング用のモーメント統計は中心3x3のみで集計する
	///      (AmbientOcclusionDenoiseCS.hlsl と同じ、広げすぎると履歴棄却が
	///      甘くなるため)。
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

			/// [EN] Background/invalid neighbors (pixels
			///      GlobalIlluminationRayGeneration wrote a=0 for) are
			///      excluded entirely.
			/// [JP] 背景/無効な近傍(GlobalIlluminationRayGeneration が a=0
			///      で書いた画素)は完全に除外する。
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
		/// [EN] No valid neighbor at all (an isolated pixel): use the raw
		///      value at this pixel as-is.
		/// [JP] 有効な近傍が一切無い(孤立ピクセル): 生の自分自身を使う。
		filtered_raw = raw_radiance.Load(int3(pixel, 0)).rgb;
	}

	/// [EN] Same as AmbientOcclusionDenoiseCS.hlsl: DenoiserVarianceClipMin/
	///      Max already internally widen the box to include filtered_raw
	///      before returning it, preventing the lower bound from overtaking
	///      the upper one downstream.
	/// [JP] AmbientOcclusionDenoiseCS.hlsl と同じく、DenoiserVarianceClipMin/
	///      Max は filtered_raw を含むまで箱を内部で広げてから返すので、
	///      下流で下限が上限を追い越すことはない。
	float3 clip_min = DenoiserVarianceClipMin(moments, filtered_raw, GI_HISTORY_CLIP_GAMMA);
	float3 clip_max = DenoiserVarianceClipMax(moments, filtered_raw, GI_HISTORY_CLIP_GAMMA);

	/// [EN] The reservoir's confidence (0..1) as written by
	///      GlobalIlluminationReservoirSpatialCS.hlsl (0 = just reset by a
	///      disocclusion, 1 = M has built up to the cap, steady state). The
	///      more converged the reservoir, the closer the blend factor moves
	///      to 1.0 (= ignore this pixel's own denoiser history and take the
	///      spatially filtered current-frame value as-is) - the reservoir
	///      has already stabilized the signal through its own temporal
	///      reuse, so stacking GI_TEMPORAL_BLEND_ALPHA's slow EMA on top of
	///      that puts two temporal filters in series, dulling the effective
	///      response a lot (the felt cause of a "sluggish drag" when panning
	///      the camera). While M stays low (right after a reset), the
	///      reservoir itself has not yet stabilized the signal, so this
	///      denoiser's own temporal blend is relied on as usual.
	/// [JP] GlobalIlluminationReservoirSpatialCS.hlsl が書いた reservoir
	///      収束度(0=直前にディスオクルージョンでリセット、1=Mが上限まで
	///      積み上がった定常状態)。収束しているほどブレンド係数を1.0
	///      (=自分の履歴を無視してフィルタ済み今フレーム値をそのまま採用)
	///      へ寄せる - reservoir が既に自前の時間的リユースで信号を安定
	///      させているので、この上にさらに GI_TEMPORAL_BLEND_ALPHA の
	///      遅いEMAを重ねると、2つの時間フィルタが直列になって実効的な
	///      応答速度が大きく鈍る(カメラを動かした時などに「ぬるっと
	///      引きずられる」体感の原因)。Mが低い間(リセット直後)は
	///      reservoir 自体がまだ信号を安定させていないので、このデノイザ
	///      自身の時間的ブレンドに通常通り頼る。
	Texture2D<float> confidence_texture = ResourceDescriptorHeap[structured_indices.global_illumination_.confidence_srv_index_];
	float confidence = confidence_texture.Load(int3(pixel, 0));
	float adaptive_temporal_alpha = lerp(GI_TEMPORAL_BLEND_ALPHA, 1.0, confidence);

	Texture2D<float4> history_radiance = ResourceDescriptorHeap[constant_indices.global_illumination_.history_srv_index_];
	float3 result = DenoiserTemporalBlend(history_radiance, previous_uv, clip_min, clip_max, filtered_raw, adaptive_temporal_alpha);

	scratch_output[pixel] = float4(result, 1.0);
}

/// [EN] Shared dispatch body for ATrousPass1/2/3. source/dest are fixed per
///      entry point (the caller, GlobalIlluminationRenderer::Dispatch,
///      dispatches all 3 in the correct order per view). Depth/normal are
///      read from the same G-Buffer as main(). Invalid pixels (background)
///      pass through unchanged with a=0.
/// [JP] ATrousPass1/2/3 共通のディスパッチ本体。source/dest はエントリ
///      ポイントごとに固定(呼び出し元 GlobalIlluminationRenderer::Dispatch
///      がビューごとに3回、正しい順序でディスパッチする)。深度/法線は
///      main() と同じ G-Buffer から読む。無効画素(背景)は a=0 のまま
///      素通しする。
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

/// [EN] Only the final pass has dest point at the real accumulated/history
///      slot (accumulated_uav_index_, ping-ponged per view every frame) -
///      note that it is no longer scratch-to-scratch here.
/// [JP] 最終パスだけ dest が本物の蓄積/履歴スロット(ビューごとに毎フレーム
///      ピンポンする accumulated_uav_index_)を指す - スクラッチ同士では
///      なくなる点に注意。
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

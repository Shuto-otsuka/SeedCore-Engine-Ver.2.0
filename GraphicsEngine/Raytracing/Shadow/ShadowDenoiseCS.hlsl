#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Denoiser.hlsli"
#include "Shadow.hlsli"

/**
* [EN]
* Reference:
* - https://research.nvidia.com/publication/2017-07_spatiotemporal-variance-guided-filtering-real-time-reconstruction-path-traced
* - https://github.com/NVIDIAGameWorks/Falcor/tree/master/Source/RenderPasses/SVGFPass
*
* SVGF (Spatiotemporal Variance-Guided Filtering, Schied et al., HPG 2017) for
* the raw stochastic shadow signal ShadowRT.hlsl produces - one binary
* visibility sample per pixel per frame, for the directional light (r) and for
* the one stochastically chosen punctual light (g).
*
* The premise of SVGF is that a denoiser should not blur by a fixed amount: it
* should blur each pixel exactly as much as that pixel is still noisy. It
* measures that by accumulating the first and second temporal moments of the
* signal per pixel, taking the variance from them, and using the variance to
* size the luminance term of the edge-stopping function in a hierarchical
* A-Trous wavelet filter. A pixel that has converged has near-zero variance,
* which collapses that term and effectively switches the filter off there; a
* pixel that was just disoccluded has high variance and gets filtered hard.
* This is what preserves a hard shadow boundary while still resolving a wide
* penumbra, which a plain depth/normal bilateral A-Trous cannot do - it has no
* notion of which pixels are already clean.
*
* Pass order (one entry point per pass, the same "one file, many entry points"
* pattern LensFlareCS.hlsl and KawaseBloomCS.hlsl use, driven by ShadowRenderer):
*   main -> FilterMoments -> ATrousPass1 -> ATrousPass2 -> ATrousPass3
* ATrousPass2 is the feedback tap: what it writes becomes next frame's temporal
* history, while ATrousPass3 produces the image DeferredLightingPS.hlsl reads.
*
* ---------------------------------------------------------------------
*
* [JP]
* ShadowRT.hlsl が生成する生の確率的シャドウ信号 — 1ピクセル1フレームあたり
* 1サンプルの二値可視性で、ディレクショナルライト(r)と確率的に選ばれた1灯の
* パンクチュアルライト(g) — に対する SVGF(Spatiotemporal Variance-Guided
* Filtering、Schied et al., HPG 2017)。
*
* SVGF の前提は「デノイザは一定量ぼかすべきではない、各ピクセルがまだ
* ノイジーな分だけぼかすべきだ」という点にある。そのためにピクセルごとに信号の
* 1次・2次モーメントを時間方向に蓄積し、そこから分散を求め、その分散で階層的な
* A-Trous ウェーブレットフィルタのエッジストッピング関数の輝度項の幅を決める。
* 収束したピクセルは分散がほぼ 0 になり、その項が潰れて実質フィルタが止まる。
* ディスオクルージョン直後のピクセルは分散が大きく強くフィルタされる。これが
* 「硬い影の境界を保ちつつ広い半影も解像する」挙動の正体であり、素の深度/法線
* バイラテラル A-Trous には出せない — あちらには「どのピクセルが既に綺麗か」
* という概念が無いため。
*
* パス順(パスごとに1エントリポイント。LensFlareCS.hlsl や KawaseBloomCS.hlsl と
* 同じ「1ファイル複数エントリポイント」パターンで、ShadowRenderer が駆動する):
*   main → FilterMoments → ATrousPass1 → ATrousPass2 → ATrousPass3
* ATrousPass2 がフィードバックタップで、その出力が次フレームの時間的履歴になる。
* DeferredLightingPS.hlsl が読む画は ATrousPass3 が書く。
*/

/// [EN] Minimum weight of the current frame in the exponential moving average
///      of the visibility, i.e. the longest history the filter will keep once
///      it has converged (1 / 0.05 = 20 frames).
/// [JP] 可視性の指数移動平均における今フレームの最小重み。収束後に保持する
///      履歴の最大長に相当する(1 / 0.05 = 20 フレーム)。
static const float SHADOW_TEMPORAL_ALPHA = 0.05;

/// [EN] Same for the luminance moments. Deliberately larger than the
///      illumination alpha: the variance estimate has to react to a change in
///      noise level faster than the signal itself converges, otherwise the
///      filter keeps blurring a region that already settled.
/// [JP] 輝度モーメント側の同じ値。意図的に本体より大きい — ノイズ量の変化には
///      信号自体の収束より速く追従する必要があり、そうしないと既に落ち着いた
///      領域をぼかし続けてしまう。
static const float SHADOW_MOMENTS_ALPHA = 0.2;

/// [EN] Cap on the accumulated frame count. Also the divisor that turns the
///      count into the "equal weight for the first frames" alpha below.
/// [JP] 蓄積フレーム数の上限。序盤フレームを等重みにする alpha の分母も兼ねる。
static const float SHADOW_MAX_HISTORY_LENGTH = 32.0;

/// [EN] Frames of history below which the temporal variance estimate is not
///      trustworthy and is replaced by a spatial one (FilterMoments).
/// [JP] この履歴長を下回ると時間的な分散推定が信用できないため、空間推定へ
///      差し替える(FilterMoments)。
static const float SHADOW_SPATIAL_VARIANCE_FRAMES = 4.0;

/// [EN] sigma_l - scale of the luminance edge-stopping term. The actual phi is
///      this times sqrt(variance), so this only sets how permissive the filter
///      is at a GIVEN noise level; a converged pixel stops filtering regardless.
/// [JP] sigma_l — 輝度エッジストッピング項のスケール。実際の phi はこれに
///      sqrt(分散) を掛けた値なので、これが決めるのは「あるノイズ量のときに
///      どこまで許すか」だけ。収束したピクセルは値に関わらずフィルタが止まる。
static const float SHADOW_PHI_LUMINANCE = 10.0;

/// [EN] sigma_n - exponent of the normal edge-stopping term.
/// [JP] sigma_n — 法線エッジストッピング項の指数。
static const float SHADOW_PHI_NORMAL = 128.0;

/// [EN] Relative depth deviation (in units of the local depth slope) beyond
///      which a reprojected pixel is treated as a different surface.
/// [JP] リプロジェクション先を別の面とみなす、深度のずれの許容量(局所的な
///      深度勾配を単位とする)。
static const float SHADOW_REPROJECT_DEPTH_TOLERANCE = 10.0;

/// [EN] Same for the normal, in units of the local normal derivative.
/// [JP] 法線側の同じ許容量(局所的な法線の変化量を単位とする)。
static const float SHADOW_REPROJECT_NORMAL_TOLERANCE = 16.0;

/// [EN] Floor under the local depth slope, as a fraction of view depth. Both
///      the reprojection test and the A-Trous depth weight divide by that
///      slope, so on a surface facing the camera - where the true slope is
///      ~0 - the quotient would explode on nothing but floating-point noise
///      and reject every sample. Expressed relative to depth rather than as
///      an absolute epsilon because the camera's far plane is 1000 units:
///      a fixed epsilon that is sane up close is far too tight in the distance.
/// [JP] 局所的な深度勾配の下限。ビュー深度に対する割合で表す。再投影テストも
///      A-Trous の深度重みもこの勾配で割るため、真の勾配が ~0 になる
///      「カメラ正対の面」では、浮動小数の誤差だけで商が発散して全サンプルが
///      棄却される。絶対値のイプシロンではなく深度比なのは、カメラの遠平面が
///      1000 単位あるため — 手前で妥当な固定値は遠方では厳しすぎる。
static const float SHADOW_DEPTH_SLOPE_FLOOR = 1e-3;

/**
* [EN]
* Loads the visibility (rg) and variance (ba) of one A-Trous source texel.
*
* ---------------------------------------------------------------------
*
* [JP]
* A-Trous ソーステクスチャ 1 テクセルの可視性(rg)と分散(ba)を読む。
*/
float4 LoadFiltered(Texture2D<float4> source, int2 pixel)
{
	return source.Load(int3(pixel, 0));
}

/**
* [EN]
* View-space depth of a pixel, or 0 for background (reverse-Z far plane).
* 0 is used as the "no surface here" marker throughout the SVGF passes, so a
* background neighbor can never be mistaken for a very distant one.
*
* ---------------------------------------------------------------------
*
* [JP]
* ピクセルのビュー空間深度。背景(reverse-Z の遠平面)は 0 を返す。
* SVGF の各パスでは 0 を「面が無い」印として使うので、背景の近傍が「とても
* 遠い面」と取り違えられることはない。
*/
float LoadViewDepth(Texture2D<float> depth_texture, SceneConstantBuffer scene, int2 pixel, int2 screen_max)
{
	int2 clamped = clamp(pixel, int2(0, 0), screen_max);
	float depth = depth_texture.Load(int3(clamped, 0));

	if (depth == 0.0)
	{
		return 0.0;
	}

	float2 uv = (float2(clamped) + 0.5) * scene.inverse_screen_size_;
	return abs(DenoiserViewPosition(scene.inverse_projection_, uv, depth).z);
}

/**
* [EN]
* SVGF's temporal consistency test for one candidate history texel: the
* reprojected surface must agree with this frame's surface in both depth and
* normal, measured against the local screen-space derivatives of each (so the
* test scales with how fast the geometry is changing across the screen rather
* than using a fixed epsilon that is too tight on slopes and too loose on flat
* walls).
*
* ---------------------------------------------------------------------
*
* [JP]
* 履歴テクセル 1 つに対する SVGF の時間的整合性テスト。リプロジェクション先の
* 面が、深度と法線の両方で今フレームの面と一致していることを要求する。判定は
* それぞれの画面上の変化率(勾配)を基準に行う — 固定のイプシロンだと斜面では
* 厳しすぎ、平らな壁では緩すぎるため。
*/
bool IsReprojectionValid(float view_z, float previous_view_z, float view_z_derivative, float3 normal, float3 previous_normal, float normal_derivative)
{
	if (previous_view_z <= 0.0)
	{
		return false;
	}

	if (abs(previous_view_z - view_z) / (view_z_derivative + view_z * SHADOW_DEPTH_SLOPE_FLOOR) > SHADOW_REPROJECT_DEPTH_TOLERANCE)
	{
		return false;
	}

	if (distance(normal, previous_normal) / (normal_derivative + 0.01) > SHADOW_REPROJECT_NORMAL_TOLERANCE)
	{
		return false;
	}

	return true;
}

/**
* [EN]
* SVGF temporal reprojection (Schied et al. 2017, section 3.1-3.2). Fetches
* last frame's filtered visibility and luminance moments at the reprojected
* position with a validity-weighted bilinear tap, falling back to a 3x3
* cross-bilateral search when all four taps fail, and integrates them with this
* frame's raw ShadowRT.hlsl sample by an exponential moving average. The blend
* factor is max(alpha, 1 / historyLength) so the first frames after a
* disocclusion weight every sample equally instead of trusting a one-frame
* history; the accumulated moments then give the per-pixel variance that drives
* the A-Trous passes below.
*
* Also writes this frame's view depth / depth derivative / normal, because the
* consistency test above needs the PREVIOUS frame's version of them and the
* engine's G-Buffer is single-buffered.
*
* ---------------------------------------------------------------------
*
* [JP]
* SVGF の時間的リプロジェクション(Schied et al. 2017 の 3.1〜3.2)。前フレームの
* フィルタ済み可視性と輝度モーメントをリプロジェクション先から取得する。取得は
* 有効性で重み付けしたバイリニアで行い、4 タップとも無効なら 3x3 の
* クロスバイラテラル探索へフォールバックする。それを今フレームの
* ShadowRT.hlsl の生サンプルと指数移動平均で統合する。ブレンド係数は
* max(alpha, 1 / 履歴長) — ディスオクルージョン直後の数フレームは 1 フレームの
* 履歴を信用せず全サンプルを等重みで扱う。こうして蓄積したモーメントが、下の
* A-Trous パスを駆動するピクセルごとの分散になる。
*
* 併せて今フレームのビュー深度 / 深度勾配 / 法線も書き出す。上の整合性テストが
* 前フレームのそれらを必要とするのに対し、エンジンの G-Buffer は
* 単一バッファのため。
*/
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	int2 pixel = int2(dtid.xy);
	int2 screen_max = int2(scene.screen_size_) - 1;

	/// [JP] raw はビュー共有(structured_indices)、蓄積チェーンはビューごと
	///      (constant_indices — Editor/Game で別バッファ)から取る。
	Texture2D<float2> raw_visibility = ResourceDescriptorHeap[structured_indices.shadow_.raw_visibility_srv_index_];
	RWTexture2D<float4> filtered_output = ResourceDescriptorHeap[constant_indices.shadow_.atrous_scratch0_uav_index_];
	RWTexture2D<float4> moments_output = ResourceDescriptorHeap[constant_indices.shadow_.moments_uav_index_];
	RWTexture2D<float> history_length_output = ResourceDescriptorHeap[constant_indices.shadow_.history_length_uav_index_];
	RWTexture2D<float4> depth_normal_output = ResourceDescriptorHeap[constant_indices.shadow_.depth_normal_uav_index_];

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		filtered_output[pixel] = float4(1.0, 1.0, 0.0, 0.0);
		moments_output[pixel] = float4(0, 0, 0, 0);
		history_length_output[pixel] = 0.0;
		depth_normal_output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	float view_z = abs(DenoiserViewPosition(scene.inverse_projection_, uv, depth).z);

	float view_z_left = LoadViewDepth(depth_texture, scene, pixel + int2(-1, 0), screen_max);
	float view_z_right = LoadViewDepth(depth_texture, scene, pixel + int2(1, 0), screen_max);
	float view_z_up = LoadViewDepth(depth_texture, scene, pixel + int2(0, -1), screen_max);
	float view_z_down = LoadViewDepth(depth_texture, scene, pixel + int2(0, 1), screen_max);
	float view_z_derivative = SvgfViewDepthDerivative(view_z_left, view_z_right, view_z_up, view_z_down);

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float3 normal = OctNormalDecode(normal_texture.Load(int3(pixel, 0)).rg);

	float3 normal_left = OctNormalDecode(normal_texture.Load(int3(clamp(pixel + int2(-1, 0), int2(0, 0), screen_max), 0)).rg);
	float3 normal_right = OctNormalDecode(normal_texture.Load(int3(clamp(pixel + int2(1, 0), int2(0, 0), screen_max), 0)).rg);
	float3 normal_up = OctNormalDecode(normal_texture.Load(int3(clamp(pixel + int2(0, -1), int2(0, 0), screen_max), 0)).rg);
	float3 normal_down = OctNormalDecode(normal_texture.Load(int3(clamp(pixel + int2(0, 1), int2(0, 0), screen_max), 0)).rg);
	float normal_derivative = max(distance(normal_right, normal_left), distance(normal_down, normal_up)) * 0.5;

	depth_normal_output[pixel] = SvgfPackDepthNormal(view_z, view_z_derivative, normal);

	float2 raw_value = raw_visibility.Load(int3(pixel, 0));

	Texture2D<float2> velocity_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_2_];
	float2 velocity = velocity_texture.Load(int3(pixel, 0));
	float2 previous_uv = DenoiserPreviousUv(uv, velocity);

	Texture2D<float4> history_visibility = ResourceDescriptorHeap[constant_indices.shadow_.history_srv_index_];
	Texture2D<float4> history_moments = ResourceDescriptorHeap[constant_indices.shadow_.moments_history_srv_index_];
	Texture2D<float> history_length_texture = ResourceDescriptorHeap[constant_indices.shadow_.history_length_history_srv_index_];
	Texture2D<float4> history_depth_normal = ResourceDescriptorHeap[constant_indices.shadow_.depth_normal_history_srv_index_];

	float2 previous_position = previous_uv * scene.screen_size_ - 0.5;
	int2 previous_base = int2(floor(previous_position));
	float2 previous_fraction = previous_position - float2(previous_base);

	float2 previous_visibility = float2(0, 0);
	float4 previous_moments = float4(0, 0, 0, 0);
	float previous_history_length = 0.0;
	bool valid = false;

	/// [JP] バイリニア 4 タップを個別に検証し、生き残ったタップだけで重みを
	///      正規化し直す。全滅なら下のクロスバイラテラル探索へ落ちる。
	{
		const int2 offsets[4] = { int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1) };
		float bilinear_weights[4] =
		{
			(1.0 - previous_fraction.x) * (1.0 - previous_fraction.y),
			previous_fraction.x * (1.0 - previous_fraction.y),
			(1.0 - previous_fraction.x) * previous_fraction.y,
			previous_fraction.x * previous_fraction.y
		};

		float weight_sum = 0.0;

		[unroll]
		for (int tap = 0; tap < 4; ++tap)
		{
			int2 tap_pixel = previous_base + offsets[tap];
			if (any(tap_pixel < int2(0, 0)) || any(tap_pixel > screen_max))
			{
				continue;
			}

			float4 tap_depth_normal = history_depth_normal.Load(int3(tap_pixel, 0));
			if (!IsReprojectionValid(view_z, tap_depth_normal.x, view_z_derivative, normal, SvgfUnpackNormal(tap_depth_normal), normal_derivative))
			{
				continue;
			}

			previous_visibility += history_visibility.Load(int3(tap_pixel, 0)).rg * bilinear_weights[tap];
			previous_moments += history_moments.Load(int3(tap_pixel, 0)) * bilinear_weights[tap];
			previous_history_length += history_length_texture.Load(int3(tap_pixel, 0)) * bilinear_weights[tap];
			weight_sum += bilinear_weights[tap];
		}

		if (weight_sum >= 0.01)
		{
			previous_visibility /= weight_sum;
			previous_moments /= weight_sum;
			previous_history_length /= weight_sum;
			valid = true;
		}
	}

	if (!valid)
	{
		/// [JP] バイリニアが全滅しても、近傍 3x3 に同じ面のテクセルが残って
		///      いることは多い。そこから拾えれば履歴を完全に捨てずに済む。
		float2 fallback_visibility = float2(0, 0);
		float4 fallback_moments = float4(0, 0, 0, 0);
		float fallback_history_length = 0.0;
		float fallback_count = 0.0;

		int2 previous_pixel = int2(previous_uv * scene.screen_size_);

		[unroll]
		for (int dy = -1; dy <= 1; ++dy)
		{
			[unroll]
			for (int dx = -1; dx <= 1; ++dx)
			{
				int2 tap_pixel = previous_pixel + int2(dx, dy);
				if (any(tap_pixel < int2(0, 0)) || any(tap_pixel > screen_max))
				{
					continue;
				}

				float4 tap_depth_normal = history_depth_normal.Load(int3(tap_pixel, 0));
				if (!IsReprojectionValid(view_z, tap_depth_normal.x, view_z_derivative, normal, SvgfUnpackNormal(tap_depth_normal), normal_derivative))
				{
					continue;
				}

				fallback_visibility += history_visibility.Load(int3(tap_pixel, 0)).rg;
				fallback_moments += history_moments.Load(int3(tap_pixel, 0));
				fallback_history_length += history_length_texture.Load(int3(tap_pixel, 0));
				fallback_count += 1.0;
			}
		}

		if (fallback_count > 0.0)
		{
			previous_visibility = fallback_visibility / fallback_count;
			previous_moments = fallback_moments / fallback_count;
			previous_history_length = fallback_history_length / fallback_count;
			valid = true;
		}
	}

	float history_length = min(SHADOW_MAX_HISTORY_LENGTH, valid ? previous_history_length + 1.0 : 1.0);

	/// [JP] 履歴が短いうちは 1/履歴長 を使う。これが指数移動平均を単純平均へ
	///      縮退させるので、序盤のフレームが不当に軽く扱われない。
	float alpha = valid ? max(SHADOW_TEMPORAL_ALPHA, 1.0 / history_length) : 1.0;
	float moments_alpha = valid ? max(SHADOW_MOMENTS_ALPHA, 1.0 / history_length) : 1.0;

	/// [JP] モーメントは (1次.x, 2次.x, 1次.y, 2次.y) の順。可視性は
	///      チャンネルごとに独立した信号なので、分散もチャンネルごとに持つ。
	float4 moments = float4(raw_value.x, raw_value.x * raw_value.x, raw_value.y, raw_value.y * raw_value.y);
	moments = lerp(previous_moments, moments, moments_alpha);

	float2 variance = max(float2(0, 0), float2(moments.y - moments.x * moments.x, moments.w - moments.z * moments.z));

	float2 visibility = lerp(previous_visibility, raw_value, alpha);

	filtered_output[pixel] = float4(visibility, variance);
	moments_output[pixel] = moments;
	history_length_output[pixel] = history_length;
}

/**
* [EN]
* Spatial variance estimate for pixels that do not have enough temporal history
* for the moment-based one to mean anything (Schied et al. 2017, section 3.3).
* Recomputes the moments over a 7x7 cross-bilateral neighborhood instead of over
* time, and boosts the result by 4 / historyLength so a one-frame-old pixel is
* filtered as aggressively as its lack of convergence warrants. Pixels with
* enough history pass through untouched.
*
* ---------------------------------------------------------------------
*
* [JP]
* モーメントによる分散推定が意味を成すだけの時間的履歴を持たないピクセル向けの
* 空間的分散推定(Schied et al. 2017 の 3.3)。時間方向ではなく 7x7 の
* クロスバイラテラル近傍でモーメントを取り直し、4 / 履歴長 を掛けて増幅する —
* 1 フレームしか経っていないピクセルは、その未収束ぶんだけ強くフィルタされる。
* 履歴が十分なピクセルはそのまま素通しする。
*/
[numthreads(8, 8, 1)]
void FilterMoments(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	int2 pixel = int2(dtid.xy);
	int2 screen_max = int2(scene.screen_size_) - 1;

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.shadow_.atrous_scratch0_srv_index_];
	Texture2D<float4> moments_texture = ResourceDescriptorHeap[constant_indices.shadow_.moments_srv_index_];
	Texture2D<float> history_length_texture = ResourceDescriptorHeap[constant_indices.shadow_.history_length_srv_index_];
	Texture2D<float4> depth_normal_texture = ResourceDescriptorHeap[constant_indices.shadow_.depth_normal_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.shadow_.atrous_scratch1_uav_index_];

	float4 center = LoadFiltered(source, pixel);
	float4 center_depth_normal = depth_normal_texture.Load(int3(pixel, 0));

	if (center_depth_normal.x <= 0.0)
	{
		dest[pixel] = center;
		return;
	}

	float history_length = history_length_texture.Load(int3(pixel, 0));

	if (history_length >= SHADOW_SPATIAL_VARIANCE_FRAMES)
	{
		dest[pixel] = center;
		return;
	}

	float3 center_normal = SvgfUnpackNormal(center_depth_normal);
	float phi_depth = max(center_depth_normal.y, center_depth_normal.x * SHADOW_DEPTH_SLOPE_FLOOR) * 3.0;
	float2 phi_luminance = float2(SHADOW_PHI_LUMINANCE, SHADOW_PHI_LUMINANCE);

	float2 weight_sum = float2(0, 0);
	float2 visibility_sum = float2(0, 0);
	float4 moments_sum = float4(0, 0, 0, 0);

	const int radius = 3;

	for (int dy = -radius; dy <= radius; ++dy)
	{
		for (int dx = -radius; dx <= radius; ++dx)
		{
			int2 tap_pixel = pixel + int2(dx, dy);
			if (any(tap_pixel < int2(0, 0)) || any(tap_pixel > screen_max))
			{
				continue;
			}

			float4 tap_depth_normal = depth_normal_texture.Load(int3(tap_pixel, 0));
			if (tap_depth_normal.x <= 0.0)
			{
				continue;
			}

			float4 tap_value = LoadFiltered(source, tap_pixel);

			float geometry_weight = SvgfDepthNormalWeight(center_depth_normal.x, tap_depth_normal.x, phi_depth * length(float2(dx, dy)), center_normal, SvgfUnpackNormal(tap_depth_normal), SHADOW_PHI_NORMAL);
			float2 weight = geometry_weight * SvgfLuminanceWeight(center.rg, tap_value.rg, phi_luminance);

			visibility_sum += tap_value.rg * weight;
			moments_sum += moments_texture.Load(int3(tap_pixel, 0)) * weight.xxyy;
			weight_sum += weight;
		}
	}

	weight_sum = max(weight_sum, float2(1e-6, 1e-6));

	visibility_sum /= weight_sum;
	moments_sum /= weight_sum.xxyy;

	float2 variance = max(float2(0, 0), float2(moments_sum.y - moments_sum.x * moments_sum.x, moments_sum.w - moments_sum.z * moments_sum.z));
	variance *= SHADOW_SPATIAL_VARIANCE_FRAMES / max(history_length, 1.0);

	dest[pixel] = float4(visibility_sum, variance);
}

/**
* [EN]
* One SVGF A-Trous wavelet iteration. Structurally this is the same "with
* holes" 5x5 kernel at a doubling step as any A-Trous filter, but the
* edge-stopping function carries the third term SVGF adds: a luminance
* difference scaled by sqrt(the pixel's own filtered variance). Because the
* variance itself is carried in the alpha channels and filtered alongside the
* signal - with SQUARED weights, since the variance of a weighted sum scales
* with the square of the weights - each iteration both blurs the signal and
* correctly shrinks its own estimate of how noisy the result still is, which is
* what makes the later, wider iterations progressively stop touching converged
* regions.
*
* ---------------------------------------------------------------------
*
* [JP]
* SVGF の A-Trous ウェーブレット 1 反復。構造としては他の A-Trous と同じ
* 「穴あき」5x5 カーネルをステップ倍々で適用するものだが、エッジストッピング
* 関数に SVGF が加える第 3 項が入る: そのピクセル自身のフィルタ済み分散の
* 平方根でスケールした輝度差。分散はアルファチャンネルに載せて信号と一緒に
* フィルタされる — 重み付き和の分散は重みの【2乗】でスケールするので、重みを
* 2乗して積む — ため、各反復は信号をぼかすと同時に「まだどれだけノイジーか」の
* 自己推定も正しく縮める。これにより後段の広いパスほど、収束済みの領域には
* 徐々に手を出さなくなる。
*/
void AtrousPassCommon(int2 pixel, Texture2D<float4> source, RWTexture2D<float4> dest, int step)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	int2 screen_max = int2(scene.screen_size_) - 1;

	Texture2D<float4> depth_normal_texture = ResourceDescriptorHeap[constant_indices.shadow_.depth_normal_srv_index_];

	float4 center = LoadFiltered(source, pixel);
	float4 center_depth_normal = depth_normal_texture.Load(int3(pixel, 0));

	if (center_depth_normal.x <= 0.0)
	{
		dest[pixel] = center;
		return;
	}

	float3 center_normal = SvgfUnpackNormal(center_depth_normal);

	float2 filtered_variance = SvgfVarianceCenter(source, pixel, screen_max);
	float2 phi_luminance = SHADOW_PHI_LUMINANCE * sqrt(max(float2(0, 0), filtered_variance + 1e-10));
	float phi_depth = max(center_depth_normal.y, center_depth_normal.x * SHADOW_DEPTH_SLOPE_FLOOR) * float(step);

	/// [JP] 中心タップは重み 1 で先に積む。エッジストッピング関数が自分自身を
	///      棄却して重み和が 0 になる事故を防ぐため。
	float2 weight_sum = float2(1, 1);
	float2 visibility_sum = center.rg;
	float2 variance_sum = center.ba;

	[unroll]
	for (int dy = -2; dy <= 2; ++dy)
	{
		[unroll]
		for (int dx = -2; dx <= 2; ++dx)
		{
			if (dx == 0 && dy == 0)
			{
				continue;
			}

			int2 tap_pixel = pixel + int2(dx, dy) * step;
			if (any(tap_pixel < int2(0, 0)) || any(tap_pixel > screen_max))
			{
				continue;
			}

			float4 tap_depth_normal = depth_normal_texture.Load(int3(tap_pixel, 0));
			if (tap_depth_normal.x <= 0.0)
			{
				continue;
			}

			float4 tap_value = LoadFiltered(source, tap_pixel);

			float kernel_weight = SVGF_ATROUS_KERNEL[abs(dx)] * SVGF_ATROUS_KERNEL[abs(dy)];
			float geometry_weight = SvgfDepthNormalWeight(center_depth_normal.x, tap_depth_normal.x, phi_depth * length(float2(dx, dy)), center_normal, SvgfUnpackNormal(tap_depth_normal), SHADOW_PHI_NORMAL);

			float2 weight = kernel_weight * geometry_weight * SvgfLuminanceWeight(center.rg, tap_value.rg, phi_luminance);

			visibility_sum += tap_value.rg * weight;
			variance_sum += tap_value.ba * weight * weight;
			weight_sum += weight;
		}
	}

	dest[pixel] = float4(visibility_sum / weight_sum, variance_sum / (weight_sum * weight_sum));
}

[numthreads(8, 8, 1)]
void ATrousPass1(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();
	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.shadow_.atrous_scratch1_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.shadow_.atrous_scratch0_uav_index_];
	AtrousPassCommon(int2(dtid.xy), source, dest, 1);
}

/**
* [EN]
* Second wavelet iteration, and the SVGF "feedback tap": its output is what
* becomes next frame's temporal history, NOT the fully filtered image. Feeding
* back the last, widest iteration would recycle its blur into the history every
* frame and compound it without bound; taking an early iteration keeps the
* history sharp while still being spatially stable enough to reproject.
*
* ---------------------------------------------------------------------
*
* [JP]
* 2 回目のウェーブレット反復であり、SVGF の「フィードバックタップ」。次フレームの
* 時間的履歴になるのは【この】出力であって、最終フィルタ結果ではない。最後の
* 一番広い反復を戻すと、そのぼけが毎フレーム履歴へ再投入されて際限なく積み
* 上がる。早い段の出力を使えば、リプロジェクションに耐える程度の空間的安定性を
* 保ちつつ履歴が鈍らない。
*/
[numthreads(8, 8, 1)]
void ATrousPass2(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();
	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.shadow_.atrous_scratch0_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.shadow_.accumulated_uav_index_];
	AtrousPassCommon(int2(dtid.xy), source, dest, 2);
}

/**
* [EN]
* Final wavelet iteration. Writes the 2-channel visibility DeferredLightingPS.hlsl
* samples; the variance channels are not needed past this point.
*
* ---------------------------------------------------------------------
*
* [JP]
* 最後のウェーブレット反復。DeferredLightingPS.hlsl がサンプルする 2 チャンネル
* 可視性を書き出す。分散チャンネルはこれ以降不要。
*/
[numthreads(8, 8, 1)]
void ATrousPass3(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();
	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	int2 pixel = int2(dtid.xy);
	int2 screen_max = int2(scene.screen_size_) - 1;

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.shadow_.accumulated_srv_index_];
	Texture2D<float4> depth_normal_texture = ResourceDescriptorHeap[constant_indices.shadow_.depth_normal_srv_index_];
	RWTexture2D<float2> dest = ResourceDescriptorHeap[constant_indices.shadow_.denoised_uav_index_];

	float4 center = LoadFiltered(source, pixel);
	float4 center_depth_normal = depth_normal_texture.Load(int3(pixel, 0));

	if (center_depth_normal.x <= 0.0)
	{
		dest[pixel] = float2(1.0, 1.0);
		return;
	}

	float3 center_normal = SvgfUnpackNormal(center_depth_normal);

	float2 filtered_variance = SvgfVarianceCenter(source, pixel, screen_max);
	float2 phi_luminance = SHADOW_PHI_LUMINANCE * sqrt(max(float2(0, 0), filtered_variance + 1e-10));
	float phi_depth = max(center_depth_normal.y, center_depth_normal.x * SHADOW_DEPTH_SLOPE_FLOOR) * 4.0;

	float2 weight_sum = float2(1, 1);
	float2 visibility_sum = center.rg;

	[unroll]
	for (int dy = -2; dy <= 2; ++dy)
	{
		[unroll]
		for (int dx = -2; dx <= 2; ++dx)
		{
			if (dx == 0 && dy == 0)
			{
				continue;
			}

			int2 tap_pixel = pixel + int2(dx, dy) * 4;
			if (any(tap_pixel < int2(0, 0)) || any(tap_pixel > screen_max))
			{
				continue;
			}

			float4 tap_depth_normal = depth_normal_texture.Load(int3(tap_pixel, 0));
			if (tap_depth_normal.x <= 0.0)
			{
				continue;
			}

			float4 tap_value = LoadFiltered(source, tap_pixel);

			float kernel_weight = SVGF_ATROUS_KERNEL[abs(dx)] * SVGF_ATROUS_KERNEL[abs(dy)];
			float geometry_weight = SvgfDepthNormalWeight(center_depth_normal.x, tap_depth_normal.x, phi_depth * length(float2(dx, dy)), center_normal, SvgfUnpackNormal(tap_depth_normal), SHADOW_PHI_NORMAL);

			float2 weight = kernel_weight * geometry_weight * SvgfLuminanceWeight(center.rg, tap_value.rg, phi_luminance);

			visibility_sum += tap_value.rg * weight;
			weight_sum += weight;
		}
	}

	dest[pixel] = saturate(visibility_sum / weight_sum);
}

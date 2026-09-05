#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Normal.hlsli"
#include "AmbientOcclusion.hlsli"

static const float AO_TEMPORAL_BLEND_ALPHA = 0.08;

/// [EN] Width of the band history is allowed to stay within (how many
///      standard errors of the mean estimator it may deviate by). Larger =
///      more ghosting survives, smaller = more noise survives.
/// [JP] 履歴を許容する範囲の幅(平均推定量の標準誤差の何倍まで許すか)。
///      大きいほど残像が残りやすく、小さいほどノイズが残る。
static const float AO_HISTORY_CLIP_SIGMA = 2.0;

/**
* [EN]
* Reference:
* - https://research.nvidia.com/publication/2017-07_spatiotemporal-variance-guided-filtering-real-time-reconstruction-path-traced
*   (Schied et al., "Spatiotemporal Variance-Guided Filtering", HPG 2017 -
*   the reprojection + neighborhood-clamp temporal scheme this file adapts to
*   a scalar occlusion signal instead of SVGF's full radiance.)
* - https://jo.dreggn.org/home/2010_atrous.pdf
*   (Dammertz et al., "Edge-Avoiding A-Trous Wavelet Transform for Fast
*   Global Illumination Filtering", HPG 2010 - the depth/normal-weighted
*   bilateral weighting this file's 5x5 spatial pass borrows, though unlike
*   ShadowDenoiseCS.hlsl/ReflectionDenoiseCS.hlsl/
*   GlobalIlluminationDenoiseCS.hlsl this file has no separate multi-pass
*   A-Trous stage of its own.)
*
* Spatio-temporal denoiser for the raw stochastic AO signal. First a 5x5
* depth-weighted bilateral average smooths the 1spp grain spatially
* (neighbors on a different surface get near-zero weight, so AO doesn't
* bleed across edges); then the previous frame's accumulated openness is
* reprojected via the G-Buffer velocity buffer, clamped to a history band
* derived from the 3x3 raw neighborhood (rejects stale history from
* disocclusion / camera cuts / the uninitialized first frame) and
* exponentially blended toward the filtered sample.
*
* [JP]
* 生の確率的AO信号の空間+時間デノイザ。まず 5x5 の深度重み付きバイラテラル
* 平均で 1spp のざらつきを空間的に均し(別の面にある近傍は重みほぼ0に
* なるのでエッジをAOが跨いで滲まない)、次に前フレームの蓄積開放度を
* G-Buffer の速度バッファでリプロジェクションし、生サンプル3x3近傍から
* 導いた履歴帯にクランプ(ディスオクルージョン・カメラカット・未初期化初回
* フレームの古い履歴を棄却)した上で、フィルタ済みサンプルへ指数ブレンド
* する。
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
	///      is per-view (constant_indices - Editor/Game use separate
	///      buffers).
	/// [JP] raw はビュー共有(structured_indices)、蓄積チェーンはビューごと
	///      (constant_indices - Editor/Game で別バッファ)から取る。
	Texture2D<float> raw_openness = ResourceDescriptorHeap[structured_indices.ambient_occlusion_.raw_srv_index_];
	RWTexture2D<float> accumulated_openness = ResourceDescriptorHeap[constant_indices.ambient_occlusion_.accumulated_uav_index_];

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		accumulated_openness[pixel] = 1.0;
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

	/// [EN] Spatial filter: smooth the raw signal with a 5x5 bilateral
	///      average before temporal integration. The weight is "plane-fit
	///      depth" + "normal agreement": a plain depth DIFFERENCE collapses
	///      the weight between neighboring pixels on the same slanted wall
	///      or floor (they are the same plane, but seen at a grazing angle
	///      their raw depths differ a lot), which effectively disables the
	///      filter and leaves the grain visible - the cause of glazing-angle
	///      surfaces looking noisier than head-on ones. Extrapolating "the
	///      depth expected if this neighbor sits on the same plane" from the
	///      local depth gradient, and judging by the deviation from THAT
	///      instead, fixes this.
	/// [JP] 空間フィルタ: 5x5 のバイラテラル平均で生ノイズを均してから
	///      時間積分する。重みは「平面フィット深度」+「法線の一致度」:
	///      単純な深度差だと、斜めから見た壁/床は同一平面なのに隣接ピクセル
	///      間の深度差が大きく、重みが潰れてフィルタが実質オフになり
	///      ノイズが残る(グレージング角の面ほどざらざらだった原因)。
	///      局所の深度勾配から「同一平面なら期待される深度」を外挿し、
	///      そこからの逸脱で判定する。
	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float3 center_normal = OctNormalDecode(normal_texture.Load(int3(pixel, 0)).rg);

	int2 screen_max = int2(scene.screen_size_) - 1;

	/// [EN] Local depth gradient (central difference) - represents the
	///      surface's tilt if it is a single plane.
	/// [JP] 局所深度勾配(中心差分)。同一平面上の傾き成分を表す。
	float depth_right = depth_texture.Load(int3(clamp(int2(pixel) + int2(1, 0), int2(0, 0), screen_max), 0));
	float depth_left = depth_texture.Load(int3(clamp(int2(pixel) + int2(-1, 0), int2(0, 0), screen_max), 0));
	float depth_down = depth_texture.Load(int3(clamp(int2(pixel) + int2(0, 1), int2(0, 0), screen_max), 0));
	float depth_up = depth_texture.Load(int3(clamp(int2(pixel) + int2(0, -1), int2(0, 0), screen_max), 0));
	float2 depth_gradient = float2(depth_right - depth_left, depth_down - depth_up) * 0.5;

	float weight_sum = 0.0;
	float weight_squared_sum = 0.0;
	float filtered_raw = 0.0;
	float neighborhood_min = 1.0;
	float neighborhood_max = 0.0;

	[unroll]
	for (int dy = -2; dy <= 2; ++dy)
	{
		[unroll]
		for (int dx = -2; dx <= 2; ++dx)
		{
			int2 neighbor = clamp(int2(pixel) + int2(dx, dy), int2(0, 0), screen_max);
			float neighbor_value = raw_openness.Load(int3(neighbor, 0));
			float neighbor_depth = depth_texture.Load(int3(neighbor, 0));

			/// [EN] Plane fit: judge "a different surface" by the deviation
			///      from the depth extrapolated from the gradient. A
			///      slanted wall seen at a grazing angle still lands on its
			///      expected depth if it's the same plane, so the weight is
			///      preserved.
			/// [JP] 平面フィット: 勾配から外挿した期待深度との差で「別の面」
			///      を判定する。斜め見の壁でも同一平面なら期待深度に乗る
			///      ので重みが保たれる。
			float expected_depth = depth + dot(depth_gradient, float2(dx, dy));
			float depth_difference = abs(neighbor_depth - expected_depth) / max(depth, 0.0001);
			float depth_weight = exp(-depth_difference * 48.0);

			/// [EN] Normal agreement: rejects a surface facing a different
			///      way even at a similar depth (the far side of an inside
			///      corner, for example).
			/// [JP] 法線の一致度: 深度が近くても向きの違う面(入隅の相手側
			///      など)は弾く。
			float3 neighbor_normal = OctNormalDecode(normal_texture.Load(int3(neighbor, 0)).rg);
			float normal_weight = pow(saturate(dot(center_normal, neighbor_normal)), 8.0);

			float weight = depth_weight * normal_weight;

			filtered_raw += neighbor_value * weight;
			weight_sum += weight;

			/// [EN] Sum of squared weights - used below for the effective
			///      sample count (Kish's effective sample size).
			/// [JP] 重みの二乗和。下の実効サンプル数(Kish)に使う。
			weight_squared_sum += weight * weight;

			/// [EN] The neighborhood clamp's range is the center 3x3 only
			///      (widening it makes history rejection too lax).
			/// [JP] 近傍クランプの範囲は中心3x3のみ(広げすぎると履歴棄却が
			///      甘くなる)。
			if (abs(dx) <= 1 && abs(dy) <= 1)
			{
				neighborhood_min = min(neighborhood_min, neighbor_value);
				neighborhood_max = max(neighborhood_max, neighbor_value);
			}
		}
	}
	filtered_raw /= max(weight_sum, 0.0001);

	/// [EN] The history's tolerance band.
	/// [JP] 履歴の許容範囲。
	///
	/// [EN] AmbientOcclusionRT.hlsl's raw signal falls off continuously with
	///      occluder distance (see that file), not a hard 0/1 - the
	///      estimator below was originally written for a strictly binary
	///      raw signal, whose worst-case (Bernoulli) variance is
	///      p*(1-p). For a value bounded to [0,1] that isn't actually
	///      binary, that Bernoulli formula is still a valid UPPER BOUND on
	///      the true variance (Bernoulli is the maximum-variance
	///      distribution on a bounded interval for a given mean), so using
	///      it here stays safe - it just means the clip band computed below
	///      can end up slightly WIDER than the true optimum in the partial-
	///      occlusion range, tolerating marginally more ghosting there than
	///      strictly necessary rather than clipping too aggressively.
	/// [JP] AmbientOcclusionRT.hlsl の生信号は遮蔽物までの距離で連続的に
	///      減衰する(同ファイル参照)- 硬い 0/1 ではない。下の推定量は
	///      元々厳密な二値信号向けに書かれたもので、その最悪ケース
	///      (ベルヌーイ)分散は p*(1-p)。[0,1] に収まる非二値の値でも、
	///      ベルヌーイの式は真の分散の有効な【上界】であり続ける(境界が
	///      あり平均が同じ分布の中でベルヌーイが分散最大)ので、ここで
	///      使い続けても安全側 - ただし部分遮蔽域では下で計算される
	///      クリップ帯が真の最適よりわずかに広くなり得る(過剰にクランプ
	///      するのではなく、そこでの残像をわずかに多めに許容する方向)。
	///
	///      A plain mean +/- gamma*sigma with a truly binary signal would
	///      let sigma widen up to 0.5 and just collapse back to [0,1], so
	///      this measures with the STANDARD ERROR OF THE MEAN instead of
	///      sigma - for a bounded [0,1] signal the variance is determined by
	///      the mean, so no separate accumulation of a second moment is
	///      needed. The Agresti-Coull-style pseudo-count (+2/+4) keeps the
	///      standard error from collapsing to 0 at edge pixels where the
	///      bilateral weights reject nearly all neighbors (effective sample
	///      count near 1), which would otherwise freeze the history onto a
	///      single noisy sample.
	///      単純な mean ± γσ は二値信号だと σ が最大 0.5 まで開いて結局
	///      [0,1] に戻るので、σ ではなく【平均の標準誤差】で測る。境界
	///      [0,1] の信号は分散が平均から決まるので、二次モーメントの
	///      蓄積は要らない。Agresti-Coull 風の擬似カウント(+2/+4)は、
	///      近傍がバイラテラル重みでほぼ棄却されて実効サンプル数が 1 に
	///      近い縁のピクセルで、標準誤差が 0 へ潰れて履歴を1点のノイズに
	///      固定してしまうのを防ぐ。
	float effective_count = (weight_sum * weight_sum) / max(weight_squared_sum, 0.0001);
	effective_count = max(effective_count, 1.0);

	float adjusted_mean = (filtered_raw * effective_count + 2.0) / (effective_count + 4.0);
	float standard_error = sqrt(adjusted_mean * (1.0 - adjusted_mean) / (effective_count + 4.0));
	float clip_radius = AO_HISTORY_CLIP_SIGMA * standard_error;

	/// [EN] Take the NARROWER of the two bands. When the neighborhood fully
	///      agrees, min/max collapse to a single point and convergence is
	///      immediate (preserving the old responsiveness at a hard occlusion
	///      boundary); in the partial range, min/max spread toward [0,1], so
	///      the confidence-interval side takes over instead.
	///
	///      min/max use the center 3x3 while filtered_raw is a 5x5 weighted
	///      average - a DIFFERENT window width - so a differing outer ring
	///      can push filtered_raw outside the 3x3 range. Intersecting the
	///      two bands as-is would then let the lower bound overtake the
	///      upper one, and clamp() would silently return the upper bound
	///      (pinning history to an unrelated value) - so the min/max band is
	///      widened to include filtered_raw before intersecting. This is a
	///      no-op in the ordinary case.
	/// [JP] 2つの範囲の【狭い方】を採る。近傍が全一致なら min/max が1点に
	///      潰れて即座に収束し(遮蔽の硬い境界での応答性は従来どおり)、
	///      中間調では min/max が [0,1] に広がるので信頼区間側が効く。
	///
	///      min/max は中心 3x3、filtered_raw は 5x5 の加重平均と【窓の幅が
	///      違う】ため、外周に別の値があると filtered_raw が 3x3 の範囲外へ
	///      出る。そのまま交差を取ると下限が上限を追い越し、clamp が黙って
	///      上限を返す(= 履歴を無関係な値へ固定する)ので、min/max 側を
	///      filtered_raw を含むまで広げてから交差させる。通常ケースでは
	///      何も変わらない。
	float band_min = min(neighborhood_min, filtered_raw);
	float band_max = max(neighborhood_max, filtered_raw);

	float clip_min = max(filtered_raw - clip_radius, band_min);
	float clip_max = min(filtered_raw + clip_radius, band_max);

	float result;
	if (previous_uv.x >= 0.0 && previous_uv.x <= 1.0 && previous_uv.y >= 0.0 && previous_uv.y <= 1.0)
	{
		Texture2D<float> history_openness = ResourceDescriptorHeap[constant_indices.ambient_occlusion_.history_srv_index_];
		float history_value = history_openness.SampleLevel(sampler_linear_clamp, previous_uv, 0);

		history_value = clamp(history_value, clip_min, clip_max);
		result = lerp(history_value, filtered_raw, AO_TEMPORAL_BLEND_ALPHA);
	}
	else
	{
		result = filtered_raw;
	}

	accumulated_openness[pixel] = result;
}

#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Denoiser.hlsli"
#include "Reflection.hlsli"

/**
* [EN]
* Reference:
* - https://research.nvidia.com/publication/2017-07_spatiotemporal-variance-guided-filtering-real-time-reconstruction-path-traced
* - https://github.com/NVIDIAGameWorks/Falcor/tree/master/Source/RenderPasses/SVGFPass
*
* SVGF (Spatiotemporal Variance-Guided Filtering, Schied et al., HPG 2017) for
* the raw 1spp GGX-importance-sampled reflection radiance ReflectionRT.hlsl
* produces - the same technique ShadowDenoiseCS.hlsl already uses successfully,
* extended from a 2-channel binary visibility signal to continuous HDR RGB
* radiance, plus a second, reflection-specific reprojection candidate.
*
* This file replaces an earlier ReBLUR-style port (NVIDIA NRD's hit-distance-
* driven blur radius and accumulation-speed confidence/antilag bookkeeping).
* That approach kept needing another confidence heuristic added on top of the
* last one to survive the next failure mode it had no way to detect (curvature
* estimation had a real singularity; the confidence/antilag system could not
* tell "the camera moved" apart from "the reflected content is a rain droplet
* that has nothing to do with last frame's", so temporally accumulating rain
* reflections smeared into streaks worse than the raw noise). SVGF sidesteps
* the whole class of problem: it does not reason about WHY history might be
* wrong, it just measures the signal's own temporal variance and clips
* reprojected history into what the CURRENT frame's neighborhood can support.
* A decorrelated signal (rain) simply keeps a high variance and gets filtered
* hard every frame instead of being trusted into a smear - the same mechanism
* that already keeps a hard shadow boundary sharp while resolving a wide
* penumbra.
*
* Two things extend the plain SVGF that Shadow uses:
*   - Dual reprojection. A reflection does not move with the surface it sits
*     on, it moves with the reflected geometry, so reprojecting along the
*     G-Buffer motion vector alone smears it under camera motion. This file
*     also reprojects a "virtual" position - the traced hit distance (carried
*     in the radiance alpha channel) extended along the surface's analytic GGX
*     specular dominant direction - and blends the two candidate UVs by
*     roughness: a smooth surface leans on the virtual (hit-point) motion,
*     where the dominant direction is a good stand-in for the one ray actually
*     traced; a rough surface falls back toward plain surface motion, where
*     the dominant direction represents any single sample poorly. This is
*     deliberately simpler than full per-pixel confidence bookkeeping - it is
*     a fixed function of roughness, not a per-frame heuristic, so it cannot
*     itself become a new source of instability.
*   - Roughness-scaled geometry tolerance. A mirror must stay essentially
*     unblurred; a rough surface's GGX footprint is wide and can be smoothed
*     hard. Both FilterMoments and the A-Trous passes scale their normal/depth
*     tolerance by roughness, so the SAME variance-guided kernel reaches wider
*     on rough surfaces and stays tight on smooth ones, instead of using a
*     hit-distance-derived radius formula.
*
* Pass order (one entry point per pass, the same "one file, many entry points"
* pattern ShadowDenoiseCS.hlsl uses, driven by ReflectionRenderer):
*   main -> FilterMoments -> ATrousPass1 -> ATrousPass2 -> ATrousPass3
* ATrousPass2 is the feedback tap: what it writes becomes next frame's
* temporal history, while ATrousPass3 produces the image
* DeferredLightingPS.hlsl reads.
*
* ---------------------------------------------------------------------
*
* [JP]
* ReflectionRT.hlsl が生成する生の 1spp GGX重点サンプリング反射放射輝度に対する
* SVGF(Spatiotemporal Variance-Guided Filtering、Schied et al., HPG 2017) —
* ShadowDenoiseCS.hlsl が既に実績を持つのと同じ技法を、2チャンネルの二値可視性
* 信号から連続値のHDR RGB放射輝度へ拡張し、反射固有の第2リプロジェクション候補を
* 加えたもの。
*
* このファイルは以前の ReBLUR型移植(NVIDIA NRD のヒット距離駆動ブラー半径と
* 蓄積速度の信頼度/アンチラグ管理)を置き換える。あちらは新しい破綻モードに
* ぶつかるたびにその上へさらに信頼度ヒューリスティックを重ねる必要があった
* (曲率推定には本物の特異点があり、信頼度/アンチラグの仕組みは「カメラが
* 動いた」と「映っている内容が雨粒で前フレームと無関係」を区別できず、雨の
* 反射を時間的に蓄積すると生ノイズより悪い縦筋になった)。SVGF はこの種の問題を
* まるごと回避する: 履歴が「なぜ」間違っているかを推論せず、信号自体の時間的
* 分散を測り、リプロジェクションした履歴を【今フレームの近傍が支持できる範囲】
* へクランプするだけ。相関の無い信号(雨)は単に高い分散を保ち続け、スメアへ
* 信用されるのではなく毎フレーム強くフィルタされる — 硬い影の境界を保ちつつ
* 広い半影も解像するのと同じ仕組み。
*
* Shadow が使う素の SVGF から2点拡張している:
*   - 二重リプロジェクション。反射は乗っている面と一緒には動かず、映っている
*     側のジオメトリと一緒に動くため、G-Buffer のモーションベクタだけで
*     リプロジェクションするとカメラ移動で滲む。このファイルはさらに「仮想」
*     位置 — トレース済みヒット距離(放射輝度のアルファに乗っている)を面の
*     解析的な GGX スペキュラ支配方向へ延長したもの — もリプロジェクションし、
*     2つの候補UVをラフネスでブレンドする: 滑らかな面は仮想(ヒット点)
*     モーションに寄る(支配方向が実際にトレースした1本の良い代役になるため)、
*     粗い面は面モーションへ戻る(支配方向が単一サンプルの代役として弱いため)。
*     これは意図的にピクセルごとの信頼度管理より単純にしてある — ラフネスの
*     固定関数であってフレームごとのヒューリスティックではないので、それ自体が
*     新たな不安定要因になりようがない。
*   - ラフネスに応じた幾何許容度。鏡面はほぼぼかしてはならず、粗い面は GGX
*     フットプリントが広いので強く均してよい。FilterMoments と A-Trous の両方が
*     法線/深度の許容度をラフネスでスケールする — ヒット距離由来の半径式では
*     なく、同じ分散誘導カーネルが粗い面ほど広く届き滑らかな面ほど締まる。
*
* パス順(パスごとに1エントリポイント。ShadowDenoiseCS.hlsl と同じ「1ファイル
* 複数エントリポイント」パターンで、ReflectionRenderer が駆動する):
*   main → FilterMoments → ATrousPass1 → ATrousPass2 → ATrousPass3
* ATrousPass2 がフィードバックタップで、その出力が次フレームの時間的履歴になる。
* DeferredLightingPS.hlsl が読む画は ATrousPass3 が書く。
*/

/// [EN] Minimum weight of the current frame in the exponential moving average
///      of the radiance, i.e. the longest history the filter will keep once
///      it has converged (1 / 0.05 = 20 frames).
/// [JP] 放射輝度の指数移動平均における今フレームの最小重み。収束後に保持する
///      履歴の最大長に相当する(1 / 0.05 = 20 フレーム)。
static const float REFLECTION_TEMPORAL_ALPHA = 0.05;

/// [EN] Same for the luminance moments. Deliberately larger than the
///      radiance alpha: the variance estimate has to react to a change in
///      noise level faster than the signal itself converges, otherwise the
///      filter keeps blurring a region that already settled.
/// [JP] 輝度モーメント側の同じ値。意図的に本体より大きい — ノイズ量の変化には
///      信号自体の収束より速く追従する必要があり、そうしないと既に落ち着いた
///      領域をぼかし続けてしまう。
static const float REFLECTION_MOMENTS_ALPHA = 0.2;

/// [EN] Cap on the accumulated frame count. Also the divisor that turns the
///      count into the "equal weight for the first frames" alpha below.
/// [JP] 蓄積フレーム数の上限。序盤フレームを等重みにする alpha の分母も兼ねる。
static const float REFLECTION_MAX_HISTORY_LENGTH = 32.0;

/// [EN] Frames of history below which the temporal variance estimate is not
///      trustworthy and is replaced by a spatial one (FilterMoments).
/// [JP] この履歴長を下回ると時間的な分散推定が信用できないため、空間推定へ
///      差し替える(FilterMoments)。
static const float REFLECTION_SPATIAL_VARIANCE_FRAMES = 4.0;

/// [EN] sigma_l - scale of the luminance edge-stopping term. The actual phi is
///      this times sqrt(variance), so this only sets how permissive the filter
///      is at a GIVEN noise level; a converged pixel stops filtering regardless.
///      Higher than ShadowDenoiseCS.hlsl's value on purpose: shadow's raw
///      signal is binary (0/1 visibility), so a small phi already lets the
///      filter treat most same-surface neighbors as "explainable by noise".
///      Reflection's raw signal is continuous glossy radiance, where the
///      per-tap luminance spread from a still-nonzero variance is larger even
///      among genuinely similar samples - too tight a phi here rejects those
///      taps as "real detail" instead of noise, leaving a residual soft
///      mottled blur that never gets swept up (the pixel's OWN variance
///      estimate is not zero, so it is not "done", but the edge-stop keeps
///      the neighbors that would finish the job out of reach).
/// [JP] sigma_l — 輝度エッジストッピング項のスケール。実際の phi はこれに
///      sqrt(分散) を掛けた値なので、これが決めるのは「あるノイズ量のときに
///      どこまで許すか」だけ。収束したピクセルは値に関わらずフィルタが止まる。
///      ShadowDenoiseCS.hlsl の値より意図的に大きい — 影の生信号は二値
///      (0/1可視性)なので、小さい phi でも「同じ面の近傍はノイズで説明できる」
///      とみなせる。反射の生信号は連続値の光沢放射輝度で、分散がまだ残って
///      いる状態では、本当は似ているサンプル同士でもタップごとの輝度の
///      ばらつきが大きくなりやすい。ここの phi が狭すぎると、そのばらつきを
///      「ノイズ」ではなく「本物のディテール」として棄却してしまい、薄い
///      まだら状のぼやけが取り切れずに残る(そのピクセル自身の分散推定は
///      0ではないので「収束済み」とは判定されないのに、それを均すはずの
///      近傍がエッジストッピングで届かない)。
static const float REFLECTION_PHI_LUMINANCE = 24.0;

/// [EN] sigma_n - exponent of the normal edge-stopping term at roughness 0
///      (see REFLECTION_ROUGHNESS_PHI_NORMAL_SCALE below for how it loosens
///      with roughness).
/// [JP] sigma_n — 法線エッジストッピング項の指数(roughness 0 のとき)。
///      ラフネスでどう緩むかは REFLECTION_ROUGHNESS_PHI_NORMAL_SCALE 参照。
static const float REFLECTION_PHI_NORMAL = 128.0;

/// [EN] Factor REFLECTION_PHI_NORMAL is scaled by at roughness 1. Lower
///      phi_normal is a LOOSER angular tolerance (SvgfDepthNormalWeight raises
///      the cosine to this power, so a smaller exponent keeps more of a
///      shallow falloff instead of collapsing to near-zero past a few
///      degrees) - which is what lets the same kernel reach further on a
///      rough surface, where the GGX footprint is physically wide, while
///      staying tight on a mirror, where blurring across so much as a
///      shallow edge would be visibly wrong. Interpolated by roughness at each
///      pixel rather than switched, so there is no seam where a material's
///      roughness varies smoothly across a surface.
/// [JP] REFLECTION_PHI_NORMAL に roughness=1 で掛かる係数。phi_normal が
///      小さいほど角度の許容度は緩む(SvgfDepthNormalWeight はこの指数で
///      コサインを累乗するため、指数が小さいほど数度を超えても緩やかにしか
///      落ちない) — 同じカーネルが、GGXフットプリントが物理的に広い粗い面では
///      遠くまで届き、わずかな傾きをぼかすだけでも明らかにおかしい鏡面では
///      締まったままになる理由。ピクセルごとにラフネスで補間する(切り替えでは
///      ない)ので、面内でラフネスが滑らかに変化しても継ぎ目が出ない。
static const float REFLECTION_ROUGHNESS_PHI_NORMAL_SCALE = 0.08;

/// [EN] Relative depth deviation (in units of the local depth slope) beyond
///      which a reprojected pixel is treated as a different surface.
/// [JP] リプロジェクション先を別の面とみなす、深度のずれの許容量(局所的な
///      深度勾配を単位とする)。
static const float REFLECTION_REPROJECT_DEPTH_TOLERANCE = 10.0;

/// [EN] Same for the normal, in units of the local normal derivative.
/// [JP] 法線側の同じ許容量(局所的な法線の変化量を単位とする)。
static const float REFLECTION_REPROJECT_NORMAL_TOLERANCE = 16.0;

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
static const float REFLECTION_DEPTH_SLOPE_FLOOR = 1e-3;

/// [EN] Roughness at which the hit-point virtual motion reprojection's weight
///      reaches zero (virtual_amount = saturate(1 - roughness / this)). Below
///      it, the two reprojection candidates blend linearly; at or above it,
///      reprojection is plain surface motion, matching ShadowDenoiseCS.hlsl.
/// [JP] ヒット点仮想モーションのリプロジェクション重みが 0 になるラフネス
///      (virtual_amount = saturate(1 - roughness / この値))。これを下回る間は
///      2つのリプロジェクション候補を線形にブレンドし、これ以上では
///      ShadowDenoiseCS.hlsl と同じ素の面モーションになる。
static const float REFLECTION_VIRTUAL_MOTION_ROUGHNESS_CUTOFF = 0.5;

/**
* [EN]
* Karis' G2 fit for how far the GGX lobe's dominant direction leans from the
* normal toward the mirror direction, and the resulting direction itself.
* Used only to place the hit-point virtual motion candidate below - not to
* drive any blur radius or kernel orientation, unlike the ReBLUR port this
* file replaces.
*
* ---------------------------------------------------------------------
*
* [JP]
* GGX ローブの支配方向が法線からミラー方向へどれだけ傾くかの Karis の G2 近似と、
* その結果の方向そのもの。下のヒット点仮想モーション候補を置くためだけに使う —
* このファイルが置き換える ReBLUR 移植と違い、ブラー半径やカーネルの向きを
* 駆動するためではない。
*/
float3 ReflectionSpecularDominantDirection(float3 normal, float3 view, float roughness)
{
	roughness = saturate(roughness);
	float normal_dot_view = abs(dot(normal, view));
	float a = 0.298475 * log(39.4115 - 39.0029 * roughness);
	float dominant_factor = saturate(pow(saturate(1.0 - normal_dot_view), 10.8649) * (1.0 - a) + a);
	float3 reflection = reflect(-view, normal);

	return normalize(lerp(normal, reflection, dominant_factor));
}

/**
* [EN]
* World-space position of the point this pixel's reflection actually shows -
* the traced hit distance (carried in the radiance alpha channel) extended
* along the analytic specular dominant direction. A straight line from a
* known point along a known direction for a known distance: no neighboring
* pixels, no differential term, no pole - it cannot become unstable regardless
* of camera motion, unlike a curvature-estimated virtual position.
*
* ---------------------------------------------------------------------
*
* [JP]
* このピクセルの反射が実際に映している点のワールド座標 — トレース済み
* ヒット距離(放射輝度のアルファに乗っている)を解析的なスペキュラ支配方向へ
* 延長するだけ。既知の点から既知の方向へ既知の距離だけ進む直線であり、隣接
* ピクセルも微分項も極も無い — 曲率推定による仮想位置と違い、カメラがどう
* 動いても不安定になりようがない。
*/
float3 ReflectionVirtualPosition(float3 world_position, float3 dominant_direction, float hit_distance)
{
	return world_position + dominant_direction * hit_distance;
}

/**
* [EN]
* SVGF's temporal consistency test for one candidate history texel (same as
* ShadowDenoiseCS.hlsl's IsReprojectionValid): the reprojected surface must
* agree with this frame's surface in both depth and normal, measured against
* the local screen-space derivatives of each.
*
* ---------------------------------------------------------------------
*
* [JP]
* 履歴テクセル1つに対する SVGF の時間的整合性テスト
* (ShadowDenoiseCS.hlsl の IsReprojectionValid と同じ)。リプロジェクション先の
* 面が、深度と法線の両方で今フレームの面と一致していることを要求する。判定は
* それぞれの画面上の変化率(勾配)を基準に行う。
*/
bool IsReflectionReprojectionValid(float view_z, float previous_view_z, float view_z_derivative, float3 normal, float3 previous_normal, float normal_derivative)
{
	if (previous_view_z <= 0.0)
	{
		return false;
	}

	if (abs(previous_view_z - view_z) / (view_z_derivative + view_z * REFLECTION_DEPTH_SLOPE_FLOOR) > REFLECTION_REPROJECT_DEPTH_TOLERANCE)
	{
		return false;
	}

	if (distance(normal, previous_normal) / (normal_derivative + 0.01) > REFLECTION_REPROJECT_NORMAL_TOLERANCE)
	{
		return false;
	}

	return true;
}

/**
* [EN]
* SVGF temporal reprojection extended with dual (surface + hit-point virtual
* motion) candidates. Computes both candidate UVs, blends them by roughness,
* then fetches last frame's filtered radiance and luminance moments at the
* blended position with a validity-weighted bilinear tap, falling back to a
* 3x3 cross-bilateral search when all four taps fail, and integrates them with
* this frame's raw ReflectionRT.hlsl sample by an exponential moving average.
* The blend factor is max(alpha, 1 / historyLength) so the first frames after
* a disocclusion weight every sample equally instead of trusting a one-frame
* history; the accumulated moments then give the per-pixel variance that
* drives the A-Trous passes below.
*
* Also writes this frame's view depth / depth derivative / normal, because the
* consistency test above needs the PREVIOUS frame's version of them and the
* engine's G-Buffer is single-buffered.
*
* ---------------------------------------------------------------------
*
* [JP]
* SVGF の時間的リプロジェクションを、二重(面+ヒット点仮想モーション)候補へ
* 拡張したもの。両方の候補UVを求め、ラフネスでブレンドし、そのブレンド位置から
* 前フレームのフィルタ済み放射輝度と輝度モーメントを取得する。取得は有効性で
* 重み付けしたバイリニアで行い、4タップとも無効なら3x3のクロスバイラテラル
* 探索へフォールバックする。それを今フレームの ReflectionRT.hlsl の生サンプルと
* 指数移動平均で統合する。ブレンド係数は max(alpha, 1/履歴長) —
* ディスオクルージョン直後の数フレームは1フレームの履歴を信用せず全サンプルを
* 等重みで扱う。こうして蓄積したモーメントが、下のA-Trousパスを駆動する
* ピクセルごとの分散になる。
*
* 併せて今フレームのビュー深度/深度勾配/法線も書き出す。上の整合性テストが
* 前フレームのそれらを必要とするのに対し、エンジンのG-Bufferは単一バッファの
* ため。
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
	Texture2D<float4> raw_radiance = ResourceDescriptorHeap[structured_indices.reflection_.output_srv_index_];
	RWTexture2D<float4> filtered_output = ResourceDescriptorHeap[constant_indices.reflection_.atrous_scratch0_uav_index_];
	RWTexture2D<float2> moments_output = ResourceDescriptorHeap[constant_indices.reflection_.moments_uav_index_];
	RWTexture2D<float> history_length_output = ResourceDescriptorHeap[constant_indices.reflection_.history_length_uav_index_];
	RWTexture2D<float4> depth_normal_output = ResourceDescriptorHeap[constant_indices.reflection_.depth_normal_uav_index_];

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		filtered_output[pixel] = float4(0, 0, 0, 0);
		moments_output[pixel] = float2(0, 0);
		history_length_output[pixel] = 0.0;
		depth_normal_output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	float3 view_position = DenoiserViewPosition(scene.inverse_projection_, uv, depth);
	float view_z = abs(view_position.z);
	float3 world_position = DenoiserWorldPosition(scene.inverse_view_projection_, uv, depth);

	float view_z_left = abs(DenoiserViewPosition(scene.inverse_projection_, (float2(clamp(pixel + int2(-1, 0), int2(0, 0), screen_max)) + 0.5) * scene.inverse_screen_size_, depth_texture.Load(int3(clamp(pixel + int2(-1, 0), int2(0, 0), screen_max), 0))).z);
	float view_z_right = abs(DenoiserViewPosition(scene.inverse_projection_, (float2(clamp(pixel + int2(1, 0), int2(0, 0), screen_max)) + 0.5) * scene.inverse_screen_size_, depth_texture.Load(int3(clamp(pixel + int2(1, 0), int2(0, 0), screen_max), 0))).z);
	float view_z_up = abs(DenoiserViewPosition(scene.inverse_projection_, (float2(clamp(pixel + int2(0, -1), int2(0, 0), screen_max)) + 0.5) * scene.inverse_screen_size_, depth_texture.Load(int3(clamp(pixel + int2(0, -1), int2(0, 0), screen_max), 0))).z);
	float view_z_down = abs(DenoiserViewPosition(scene.inverse_projection_, (float2(clamp(pixel + int2(0, 1), int2(0, 0), screen_max)) + 0.5) * scene.inverse_screen_size_, depth_texture.Load(int3(clamp(pixel + int2(0, 1), int2(0, 0), screen_max), 0))).z);
	float view_z_derivative = SvgfViewDepthDerivative(view_z_left, view_z_right, view_z_up, view_z_down);

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float4 gbuffer1 = normal_texture.Load(int3(pixel, 0));
	float3 normal = OctNormalDecode(gbuffer1.rg);
	float roughness = gbuffer1.b;

	float3 normal_left = OctNormalDecode(normal_texture.Load(int3(clamp(pixel + int2(-1, 0), int2(0, 0), screen_max), 0)).rg);
	float3 normal_right = OctNormalDecode(normal_texture.Load(int3(clamp(pixel + int2(1, 0), int2(0, 0), screen_max), 0)).rg);
	float3 normal_up = OctNormalDecode(normal_texture.Load(int3(clamp(pixel + int2(0, -1), int2(0, 0), screen_max), 0)).rg);
	float3 normal_down = OctNormalDecode(normal_texture.Load(int3(clamp(pixel + int2(0, 1), int2(0, 0), screen_max), 0)).rg);
	float normal_derivative = max(distance(normal_right, normal_left), distance(normal_down, normal_up)) * 0.5;

	depth_normal_output[pixel] = SvgfPackDepthNormal(view_z, view_z_derivative, normal);

	float4 raw_value = raw_radiance.Load(int3(pixel, 0));

	Texture2D<float2> velocity_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_2_];
	float2 velocity = velocity_texture.Load(int3(pixel, 0));
	float2 surface_uv = DenoiserPreviousUv(uv, velocity);

	/// [JP] ヒット点仮想モーション候補。トレース済みヒット距離(raw_value.a)を
	///      面のスペキュラ支配方向へ延長した点を、前フレームの
	///      view-projection でリプロジェクションする。
	float3 view_direction = normalize(world_position - scene.camera_position_.xyz);
	float3 dominant_direction = ReflectionSpecularDominantDirection(normal, -view_direction, roughness);
	float3 virtual_position = ReflectionVirtualPosition(world_position, dominant_direction, raw_value.a);

	float4 virtual_clip = mul(float4(virtual_position, 1.0), scene.previous_view_projection_);
	float2 virtual_ndc = virtual_clip.xy / max(virtual_clip.w, 1e-6);
	float2 virtual_uv = float2(virtual_ndc.x * 0.5 + 0.5, 0.5 - virtual_ndc.y * 0.5);

	/// [JP] 2候補をラフネスでブレンドする。フレームごとの信頼度判定ではなく
	///      ラフネスの固定関数 — REFLECTION_VIRTUAL_MOTION_ROUGHNESS_CUTOFF 参照。
	float virtual_amount = saturate(1.0 - roughness / REFLECTION_VIRTUAL_MOTION_ROUGHNESS_CUTOFF);
	float2 previous_uv = lerp(surface_uv, virtual_uv, virtual_amount);

	Texture2D<float4> history_radiance = ResourceDescriptorHeap[constant_indices.reflection_.history_srv_index_];
	Texture2D<float2> history_moments = ResourceDescriptorHeap[constant_indices.reflection_.moments_history_srv_index_];
	Texture2D<float> history_length_texture = ResourceDescriptorHeap[constant_indices.reflection_.history_length_history_srv_index_];
	Texture2D<float4> history_depth_normal = ResourceDescriptorHeap[constant_indices.reflection_.depth_normal_history_srv_index_];

	float2 previous_position = previous_uv * scene.screen_size_ - 0.5;
	int2 previous_base = int2(floor(previous_position));
	float2 previous_fraction = previous_position - float2(previous_base);

	float3 previous_radiance = float3(0, 0, 0);
	float previous_variance = 0.0;
	float2 previous_moments = float2(0, 0);
	float previous_history_length = 0.0;
	bool valid = false;

	/// [JP] バイリニア4タップを個別に検証し、生き残ったタップだけで重みを
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
			if (!IsReflectionReprojectionValid(view_z, tap_depth_normal.x, view_z_derivative, normal, SvgfUnpackNormal(tap_depth_normal), normal_derivative))
			{
				continue;
			}

			float4 tap_radiance = history_radiance.Load(int3(tap_pixel, 0));
			previous_radiance += tap_radiance.rgb * bilinear_weights[tap];
			previous_variance += tap_radiance.a * bilinear_weights[tap];
			previous_moments += history_moments.Load(int3(tap_pixel, 0)) * bilinear_weights[tap];
			previous_history_length += history_length_texture.Load(int3(tap_pixel, 0)) * bilinear_weights[tap];
			weight_sum += bilinear_weights[tap];
		}

		if (weight_sum >= 0.01)
		{
			previous_radiance /= weight_sum;
			previous_variance /= weight_sum;
			previous_moments /= weight_sum;
			previous_history_length /= weight_sum;
			valid = true;
		}
	}

	if (!valid)
	{
		/// [JP] バイリニアが全滅しても、近傍3x3に同じ面のテクセルが残って
		///      いることは多い。そこから拾えれば履歴を完全に捨てずに済む。
		float3 fallback_radiance = float3(0, 0, 0);
		float fallback_variance = 0.0;
		float2 fallback_moments = float2(0, 0);
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
				if (!IsReflectionReprojectionValid(view_z, tap_depth_normal.x, view_z_derivative, normal, SvgfUnpackNormal(tap_depth_normal), normal_derivative))
				{
					continue;
				}

				float4 tap_radiance = history_radiance.Load(int3(tap_pixel, 0));
				fallback_radiance += tap_radiance.rgb;
				fallback_variance += tap_radiance.a;
				fallback_moments += history_moments.Load(int3(tap_pixel, 0));
				fallback_history_length += history_length_texture.Load(int3(tap_pixel, 0));
				fallback_count += 1.0;
			}
		}

		if (fallback_count > 0.0)
		{
			previous_radiance = fallback_radiance / fallback_count;
			previous_variance = fallback_variance / fallback_count;
			previous_moments = fallback_moments / fallback_count;
			previous_history_length = fallback_history_length / fallback_count;
			valid = true;
		}
	}

	float history_length = min(REFLECTION_MAX_HISTORY_LENGTH, valid ? previous_history_length + 1.0 : 1.0);

	/// [JP] 履歴が短いうちは 1/履歴長 を使う。これが指数移動平均を単純平均へ
	///      縮退させるので、序盤のフレームが不当に軽く扱われない。
	float alpha = valid ? max(REFLECTION_TEMPORAL_ALPHA, 1.0 / history_length) : 1.0;
	float moments_alpha = valid ? max(REFLECTION_MOMENTS_ALPHA, 1.0 / history_length) : 1.0;

	float raw_luminance = dot(raw_value.rgb, float3(0.2126, 0.7152, 0.0722));

	float2 moments = float2(raw_luminance, raw_luminance * raw_luminance);
	moments = lerp(previous_moments, moments, moments_alpha);

	float variance = max(0.0, moments.y - moments.x * moments.x);

	float3 radiance = lerp(previous_radiance, raw_value.rgb, alpha);

	filtered_output[pixel] = float4(radiance, variance);
	moments_output[pixel] = moments;
	history_length_output[pixel] = history_length;
}

/**
* [EN]
* Spatial variance estimate for pixels that do not have enough temporal
* history for the moment-based one to mean anything (Schied et al. 2017,
* section 3.3). Recomputes the moments over a 7x7 cross-bilateral neighborhood
* instead of over time, and boosts the result by
* REFLECTION_SPATIAL_VARIANCE_FRAMES / historyLength so a one-frame-old pixel
* is filtered as aggressively as its lack of convergence warrants. Pixels with
* enough history pass through untouched.
*
* ---------------------------------------------------------------------
*
* [JP]
* モーメントによる分散推定が意味を成すだけの時間的履歴を持たないピクセル向けの
* 空間的分散推定(Schied et al. 2017 の 3.3)。時間方向ではなく 7x7 の
* クロスバイラテラル近傍でモーメントを取り直し、
* REFLECTION_SPATIAL_VARIANCE_FRAMES / 履歴長 を掛けて増幅する — 1フレームしか
* 経っていないピクセルは、その未収束ぶんだけ強くフィルタされる。履歴が十分な
* ピクセルはそのまま素通しする。
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

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.reflection_.atrous_scratch0_srv_index_];
	Texture2D<float2> moments_texture = ResourceDescriptorHeap[constant_indices.reflection_.moments_srv_index_];
	Texture2D<float> history_length_texture = ResourceDescriptorHeap[constant_indices.reflection_.history_length_srv_index_];
	Texture2D<float4> depth_normal_texture = ResourceDescriptorHeap[constant_indices.reflection_.depth_normal_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.reflection_.atrous_scratch1_uav_index_];

	float4 center = source.Load(int3(pixel, 0));
	float4 center_depth_normal = depth_normal_texture.Load(int3(pixel, 0));

	if (center_depth_normal.x <= 0.0)
	{
		dest[pixel] = center;
		return;
	}

	float history_length = history_length_texture.Load(int3(pixel, 0));

	if (history_length >= REFLECTION_SPATIAL_VARIANCE_FRAMES)
	{
		dest[pixel] = center;
		return;
	}

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float roughness = normal_texture.Load(int3(pixel, 0)).b;
	float phi_normal = lerp(REFLECTION_PHI_NORMAL, REFLECTION_PHI_NORMAL * REFLECTION_ROUGHNESS_PHI_NORMAL_SCALE, roughness);

	float3 center_normal = SvgfUnpackNormal(center_depth_normal);
	float phi_depth = max(center_depth_normal.y, center_depth_normal.x * REFLECTION_DEPTH_SLOPE_FLOOR) * 3.0;
	float phi_luminance = REFLECTION_PHI_LUMINANCE;

	float weight_sum = 0.0;
	float3 radiance_sum = float3(0, 0, 0);
	float2 moments_sum = float2(0, 0);

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

			float4 tap_value = source.Load(int3(tap_pixel, 0));

			float tap_luminance = dot(tap_value.rgb, float3(0.2126, 0.7152, 0.0722));
			float center_luminance = dot(center.rgb, float3(0.2126, 0.7152, 0.0722));

			float geometry_weight = SvgfDepthNormalWeight(center_depth_normal.x, tap_depth_normal.x, phi_depth * length(float2(dx, dy)), center_normal, SvgfUnpackNormal(tap_depth_normal), phi_normal);
			float weight = geometry_weight * exp(-abs(center_luminance - tap_luminance) / phi_luminance);

			radiance_sum += tap_value.rgb * weight;
			moments_sum += moments_texture.Load(int3(tap_pixel, 0)) * weight;
			weight_sum += weight;
		}
	}

	weight_sum = max(weight_sum, 1e-6);

	radiance_sum /= weight_sum;
	moments_sum /= weight_sum;

	float variance = max(0.0, moments_sum.y - moments_sum.x * moments_sum.x);
	variance *= REFLECTION_SPATIAL_VARIANCE_FRAMES / max(history_length, 1.0);

	dest[pixel] = float4(radiance_sum, variance);
}

/**
* [EN]
* One SVGF A-Trous wavelet iteration. Structurally this is the same "with
* holes" 5x5 kernel at a doubling step as any A-Trous filter, but the
* edge-stopping function carries the third term SVGF adds: a luminance
* difference scaled by sqrt(the pixel's own filtered variance), and the
* geometry term's normal tolerance is scaled by this pixel's roughness (see
* REFLECTION_ROUGHNESS_PHI_NORMAL_SCALE). Because the variance itself is
* carried in the alpha channel and filtered alongside the signal - with
* SQUARED weights, since the variance of a weighted sum scales with the square
* of the weights - each iteration both blurs the signal and correctly shrinks
* its own estimate of how noisy the result still is, which is what makes the
* later, wider iterations progressively stop touching converged regions.
*
* ---------------------------------------------------------------------
*
* [JP]
* SVGF の A-Trous ウェーブレット1反復。構造としては他の A-Trous と同じ
* 「穴あき」5x5 カーネルをステップ倍々で適用するものだが、エッジストッピング
* 関数に SVGF が加える第3項が入る: そのピクセル自身のフィルタ済み分散の
* 平方根でスケールした輝度差。さらに幾何項の法線許容度はこのピクセルの
* ラフネスでスケールする(REFLECTION_ROUGHNESS_PHI_NORMAL_SCALE 参照)。分散は
* アルファチャンネルに載せて信号と一緒にフィルタされる — 重み付き和の分散は
* 重みの【2乗】でスケールするので、重みを2乗して積む — ため、各反復は信号を
* ぼかすと同時に「まだどれだけノイジーか」の自己推定も正しく縮める。これに
* より後段の広いパスほど、収束済みの領域には徐々に手を出さなくなる。
*/
void AtrousPassCommon(int2 pixel, Texture2D<float4> source, RWTexture2D<float4> dest, int step)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	int2 screen_max = int2(scene.screen_size_) - 1;

	Texture2D<float4> depth_normal_texture = ResourceDescriptorHeap[constant_indices.reflection_.depth_normal_srv_index_];

	float4 center = source.Load(int3(pixel, 0));
	float4 center_depth_normal = depth_normal_texture.Load(int3(pixel, 0));

	if (center_depth_normal.x <= 0.0)
	{
		dest[pixel] = center;
		return;
	}

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float roughness = normal_texture.Load(int3(pixel, 0)).b;
	float phi_normal = lerp(REFLECTION_PHI_NORMAL, REFLECTION_PHI_NORMAL * REFLECTION_ROUGHNESS_PHI_NORMAL_SCALE, roughness);

	float3 center_normal = SvgfUnpackNormal(center_depth_normal);

	float filtered_variance = SvgfVarianceCenter(source, pixel, screen_max).x;
	float phi_luminance = REFLECTION_PHI_LUMINANCE * sqrt(max(0.0, filtered_variance) + 1e-10);
	float phi_depth = max(center_depth_normal.y, center_depth_normal.x * REFLECTION_DEPTH_SLOPE_FLOOR) * float(step);

	/// [JP] 中心タップは重み1で先に積む。エッジストッピング関数が自分自身を
	///      棄却して重み和が0になる事故を防ぐため。
	float weight_sum = 1.0;
	float3 radiance_sum = center.rgb;
	float variance_sum = center.a;

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

			float4 tap_value = source.Load(int3(tap_pixel, 0));

			float tap_luminance = dot(tap_value.rgb, float3(0.2126, 0.7152, 0.0722));
			float center_luminance = dot(center.rgb, float3(0.2126, 0.7152, 0.0722));

			float kernel_weight = SVGF_ATROUS_KERNEL[abs(dx)] * SVGF_ATROUS_KERNEL[abs(dy)];
			float geometry_weight = SvgfDepthNormalWeight(center_depth_normal.x, tap_depth_normal.x, phi_depth * length(float2(dx, dy)), center_normal, SvgfUnpackNormal(tap_depth_normal), phi_normal);

			float weight = kernel_weight * geometry_weight * exp(-abs(center_luminance - tap_luminance) / phi_luminance);

			radiance_sum += tap_value.rgb * weight;
			variance_sum += tap_value.a * weight * weight;
			weight_sum += weight;
		}
	}

	dest[pixel] = float4(radiance_sum / weight_sum, variance_sum / (weight_sum * weight_sum));
}

[numthreads(8, 8, 1)]
void ATrousPass1(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();
	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.reflection_.atrous_scratch1_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.reflection_.atrous_scratch0_uav_index_];
	AtrousPassCommon(int2(dtid.xy), source, dest, 1);
}

/**
* [EN]
* Second wavelet iteration, and the SVGF "feedback tap": its output is what
* becomes next frame's temporal history, NOT the fully filtered image. Feeding
* back the last, widest iteration would recycle its blur into the history
* every frame and compound it without bound; taking an early iteration keeps
* the history sharp while still being spatially stable enough to reproject.
*
* ---------------------------------------------------------------------
*
* [JP]
* 2回目のウェーブレット反復であり、SVGF の「フィードバックタップ」。次フレームの
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

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.reflection_.atrous_scratch0_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.reflection_.accumulated_uav_index_];
	AtrousPassCommon(int2(dtid.xy), source, dest, 2);
}

/**
* [EN]
* Final wavelet iteration. Writes the RGBA16F image DeferredLightingPS.hlsl
* samples - rgb = filtered radiance, a = 1.0 (valid) / 0.0 (background), the
* same validity convention GlobalIlluminationRT.hlsl and
* AmbientOcclusionRT.hlsl use. The variance channel is not needed past this
* point.
*
* ---------------------------------------------------------------------
*
* [JP]
* 最後のウェーブレット反復。DeferredLightingPS.hlsl がサンプルする RGBA16F の
* 画を書き出す — rgb = フィルタ済み放射輝度、a = 1.0(有効)/ 0.0(背景)。
* GlobalIlluminationRT.hlsl や AmbientOcclusionRT.hlsl と同じ有効フラグの規約。
* 分散チャンネルはこれ以降不要。
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

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.reflection_.accumulated_srv_index_];
	Texture2D<float4> depth_normal_texture = ResourceDescriptorHeap[constant_indices.reflection_.depth_normal_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.reflection_.denoised_uav_index_];

	float4 center = source.Load(int3(pixel, 0));
	float4 center_depth_normal = depth_normal_texture.Load(int3(pixel, 0));

	if (center_depth_normal.x <= 0.0)
	{
		dest[pixel] = float4(0, 0, 0, 0);
		return;
	}

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float roughness = normal_texture.Load(int3(pixel, 0)).b;
	float phi_normal = lerp(REFLECTION_PHI_NORMAL, REFLECTION_PHI_NORMAL * REFLECTION_ROUGHNESS_PHI_NORMAL_SCALE, roughness);

	float3 center_normal = SvgfUnpackNormal(center_depth_normal);

	float filtered_variance = SvgfVarianceCenter(source, pixel, screen_max).x;
	float phi_luminance = REFLECTION_PHI_LUMINANCE * sqrt(max(0.0, filtered_variance) + 1e-10);
	float phi_depth = max(center_depth_normal.y, center_depth_normal.x * REFLECTION_DEPTH_SLOPE_FLOOR) * 4.0;

	float weight_sum = 1.0;
	float3 radiance_sum = center.rgb;

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

			float4 tap_value = source.Load(int3(tap_pixel, 0));

			float tap_luminance = dot(tap_value.rgb, float3(0.2126, 0.7152, 0.0722));
			float center_luminance = dot(center.rgb, float3(0.2126, 0.7152, 0.0722));

			float kernel_weight = SVGF_ATROUS_KERNEL[abs(dx)] * SVGF_ATROUS_KERNEL[abs(dy)];
			float geometry_weight = SvgfDepthNormalWeight(center_depth_normal.x, tap_depth_normal.x, phi_depth * length(float2(dx, dy)), center_normal, SvgfUnpackNormal(tap_depth_normal), phi_normal);

			float weight = kernel_weight * geometry_weight * exp(-abs(center_luminance - tap_luminance) / phi_luminance);

			radiance_sum += tap_value.rgb * weight;
			weight_sum += weight;
		}
	}

	dest[pixel] = float4(DenoiserSanitizeRadiance(radiance_sum / weight_sum), 1.0);
}

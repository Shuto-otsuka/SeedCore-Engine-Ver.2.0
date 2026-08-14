#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Denoiser.hlsli"
#include "Reflection.hlsli"

/**
* [EN]
* Reference:
* - https://github.com/NVIDIA-RTX/NRD
* - https://link.springer.com/chapter/10.1007/978-1-4842-7185-8_49
* - https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf
*
* ReBLUR (NVIDIA Real-time Denoisers' hierarchical recurrent denoiser) for the
* raw 1spp GGX-importance-sampled reflection radiance ReflectionRT.hlsl
* produces. ReflectionRT.hlsl packs the ray's normalized hit distance into the
* alpha channel, because that hit distance is the signal this whole denoiser is
* built around.
*
* The premise of ReBLUR is that for specular the right filter width is a
* geometric quantity, not a statistical one. How blurry a reflection is allowed
* to be follows from how far the reflected thing actually is: a short ray means
* the reflection sits close to the surface (a contact reflection) and must stay
* sharp, while a long ray means the GGX lobe's footprint at the hit point is
* wide and the result can be blurred heavily without losing anything real. That
* is why the blur radius here is derived from hit distance, roughness and the
* number of accumulated frames rather than from a measured variance, and why
* the kernel is a world-space ellipse oriented along the specular dominant
* direction instead of an isotropic screen-space disk - the blur has to follow
* the shape of the lobe it is reconstructing.
*
* The other half of ReBLUR is virtual motion. A reflection does not move with
* the surface it is on, it moves with the geometry being reflected, so
* reprojecting specular history along the G-Buffer motion vector smears it
* under camera motion. TemporalAccumulation therefore also reprojects a virtual
* position placed behind the surface by the thin-lens equation (using the local
* curvature) and blends between the two reprojections by how much history each
* of them can justify.
*
* Pass order (one entry point per pass, the same "one file, many entry points"
* pattern LensFlareCS.hlsl and KawaseBloomCS.hlsl use, driven by
* ReflectionRenderer):
*   PrePass -> main (TemporalAccumulation) -> HistoryFix -> Blur -> PostBlur
*
* ---------------------------------------------------------------------
*
* [JP]
* ReflectionRT.hlsl が生成する生の 1spp GGX重点サンプリング反射放射輝度に対する
* ReBLUR(NVIDIA Real-time Denoisers の階層的リカレントデノイザ)。
* ReflectionRT.hlsl はレイの正規化ヒット距離をアルファチャンネルへ詰めている —
* このデノイザ全体がそのヒット距離を軸に組み立てられているため。
*
* ReBLUR の前提は「スペキュラにおける適切なフィルタ幅は統計量ではなく幾何量で
* ある」という点にある。反射をどれだけぼかしてよいかは、映っている対象が実際に
* どれだけ遠いかから決まる。レイが短ければ反射は面の近くにある(接触反射)ので
* 鋭いままでなければならず、レイが長ければヒット点での GGX ローブのフットプリント
* が広いので、実在する情報を失わずに大きくぼかせる。ブラー半径を実測分散ではなく
* ヒット距離・ラフネス・蓄積フレーム数から導くのはそのためであり、カーネルが
* 等方な画面空間の円ではなくスペキュラ支配方向に沿ったワールド空間の楕円なのも
* 同じ理由 — ぼかしは再構成しようとしているローブの形に従う必要がある。
*
* ReBLUR のもう半分が仮想モーション。反射は乗っている面と一緒には動かず、映って
* いる側のジオメトリと一緒に動く。そのため G-Buffer のモーションベクタだけで
* スペキュラ履歴をリプロジェクションするとカメラ移動で滲む。そこで
* TemporalAccumulation では、局所曲率を使った薄レンズの式で面の裏側に置いた
* 仮想位置もリプロジェクションし、2つのリプロジェクションを「どちらがどれだけ
* 履歴を正当化できるか」でブレンドする。
*
* パス順(パスごとに1エントリポイント。LensFlareCS.hlsl や KawaseBloomCS.hlsl と
* 同じ「1ファイル複数エントリポイント」パターンで、ReflectionRenderer が駆動):
*   PrePass → main(TemporalAccumulation) → HistoryFix → Blur → PostBlur
*/

/// [EN] nrd::ReblurHitDistanceParameters - (constant, viewZ scale, roughness
///      scale) of the hit distance normalization. Must match the value
///      ReflectionRT.hlsl normalizes with, or every distance-derived radius in
///      this file is wrong by that ratio.
/// [JP] nrd::ReblurHitDistanceParameters — ヒット距離正規化の(定数、viewZ係数、
///      ラフネス係数)。ReflectionRT.hlsl が正規化に使う値と一致させること。
///      ずれるとこのファイル中のヒット距離由来の半径が全てその比率だけ狂う。
static const float3 REFLECTION_HIT_DISTANCE_PARAMS = float3(3.0, 0.1, 20.0);

/// [EN] Longest history ReBLUR will accumulate, in frames.
/// [JP] ReBLUR が蓄積する履歴の最大フレーム数。
static const float REFLECTION_MAX_ACCUM_FRAME_NUM = 30.0;

/// [EN] Length of the SHORT history used only to keep the long one honest. A
///      30-frame exponential history weights the current frame at 1/31, so once
///      converged it barely moves; if the reprojection is even slightly off -
///      and for specular it always is, since virtual motion is an approximation
///      - the image visibly trails the camera with nothing to pull it back.
///      ReBLUR's answer is this second, much shorter history: it tracks motion
///      almost immediately, and the slow history is clamped into its local luma
///      box every frame, so lag can never exceed what this one exhibits.
/// [JP] 長い履歴を暴走させないためだけに持つ【短い】履歴の長さ。30フレームの
///      指数履歴は今フレームの重みが 1/31 しかないため、収束後はほとんど動かず、
///      リプロジェクションがわずかでもずれていれば — スペキュラの仮想モーションは
///      近似なので常にずれる — 引き戻す力が無く、絵がカメラに対して目に見えて
///      遅れる。ReBLUR の答えがこの2本目の短い履歴で、こちらはほぼ即座に動きへ
///      追従し、遅い履歴は毎フレームその局所ルミナンス箱へクランプされる。
///      結果としてラグはこの短い履歴のラグを超えられない。
static const float REFLECTION_MAX_FAST_ACCUM_FRAME_NUM = 6.0;

/// [EN] Half-width of that clamping box, in standard deviations of the current
///      frame's local luma. Too tight reintroduces the 1spp noise the long
///      history exists to remove; too loose stops constraining lag at all.
/// [JP] そのクランプ箱の半幅。今フレームの局所ルミナンスの標準偏差を単位とする。
///      狭すぎると長い履歴が消していた 1spp ノイズが戻り、広すぎるとラグを
///      まったく拘束しなくなる。
static const float REFLECTION_FAST_HISTORY_CLAMP_SIGMA = 2.0;

/// [EN] Frames below which a pixel is considered to have no usable history and
///      is spatially reconstructed by the HistoryFix pass instead.
/// [JP] これを下回ると「使える履歴が無い」とみなし、HistoryFix パスで空間的に
///      再構成する。
static const float REFLECTION_HISTORY_FIX_FRAME_NUM = 3.0;

/// [EN] Base pixel stride of the HistoryFix 5x5 reconstruction kernel. Large on
///      purpose: it has to reach far enough to find ANY converged neighbor.
/// [JP] HistoryFix の 5x5 再構成カーネルの基準ピクセル間隔。意図的に大きい —
///      収束済みの近傍を「とにかく見つける」ために遠くまで届く必要がある。
static const float REFLECTION_HISTORY_FIX_STRIDE = 14.0;

/// [EN] Max / min spatial filter radius in pixels. The max is what a fully
///      unconverged rough pixel with a distant hit gets; the min is the floor a
///      converged one keeps so it never degenerates to a pure point sample.
/// [JP] 空間フィルタ半径の最大/最小(ピクセル)。最大は「完全に未収束で粗く、
///      遠くにヒットした」ピクセルに適用される値。最小は収束済みピクセルが保つ
///      下限で、純粋な点サンプルへ縮退させないためのもの。
static const float REFLECTION_MAX_BLUR_RADIUS = 30.0;
static const float REFLECTION_MIN_BLUR_RADIUS = 1.0;

/// [EN] Pre-pass blur radius. Separate (and larger) from the max above because
///      the pre-pass runs before any temporal accumulation exists and its only
///      job is to break up the 1spp boiling enough for reprojection to track.
/// [JP] プリパスのブラー半径。上の最大値とは別で、かつ大きい — プリパスは時間
///      蓄積が存在する前に走り、その仕事は 1spp の沸き立ちを、リプロジェクション
///      が追従できる程度まで潰すことだけだから。
static const float REFLECTION_PREPASS_BLUR_RADIUS = 50.0;

/// [EN] Fraction of the specular lobe angle used for normal-based rejection.
/// [JP] 法線による棄却に使うスペキュラローブ角の割合。
static const float REFLECTION_LOBE_ANGLE_FRACTION = 0.15;

/// [EN] Fraction of the center roughness used for roughness-based rejection.
/// [JP] ラフネスによる棄却に使う中心ラフネスの割合。
static const float REFLECTION_ROUGHNESS_FRACTION = 0.15;

/// [EN] Maximum allowed deviation from the center pixel's tangent plane, as a
///      fraction of the frustum size at that depth.
/// [JP] 中心ピクセルの接平面から許容する最大のずれ。その深度における視錐台
///      サイズに対する割合で表す。
static const float REFLECTION_PLANE_DISTANCE_SENSITIVITY = 0.02;

/// [EN] Floor of the hit-distance weight. Without a floor, a pixel whose
///      neighbors all reflect something at a different distance would reject
///      every tap and keep its raw 1spp noise.
/// [JP] ヒット距離重みの下限。下限が無いと、近傍が全て別の距離を映している
///      ピクセルは全タップを棄却し、生の 1spp ノイズを抱えたまま残る。
static const float REFLECTION_MIN_HIT_DISTANCE_WEIGHT = 0.1;

/// [EN] Scale of the pre-pass / blur / post-blur radius and of the weight
///      relaxation at each stage. The post-blur reaches twice as far with half
///      the tolerance, so the hierarchy widens spatially while tightening on
///      what it is allowed to mix.
/// [JP] プリパス/ブラー/ポストブラーそれぞれの半径と重み緩和のスケール。
///      ポストブラーは 2 倍の距離まで届きつつ許容度は半分になる — 階層は空間的
///      には広がりながら、混ぜてよい相手については逆に厳しくなる。
static const float REFLECTION_PREPASS_RADIUS_SCALE = 1.0;
static const float REFLECTION_PREPASS_FRACTION_SCALE = 2.0;
static const float REFLECTION_BLUR_RADIUS_SCALE = 1.0;
static const float REFLECTION_BLUR_FRACTION_SCALE = 1.0;
static const float REFLECTION_POST_BLUR_RADIUS_SCALE = 2.0;
static const float REFLECTION_POST_BLUR_FRACTION_SCALE = 0.5;

/// [EN] Non-linear accumulation speed the pre-pass pretends to have. The
///      pre-pass runs before accumulation, so it borrows the weights of a
///      pixel with ~10 frames of history to avoid being either wide open or
///      fully closed.
/// [JP] プリパスが仮定する非線形蓄積速度。プリパスは蓄積の前に走るため、履歴
///      約10フレーム相当のピクセルの重みを借りる — 全開でも全閉でもない中間へ
///      落ち着かせるため。
static const float REFLECTION_PREPASS_NON_LINEAR_ACCUM_SPEED = 1.0 / 11.0;

/// [EN] Suppression of sporadic bright outliers: how many times the history
///      luminance a single accumulated sample may reach before being clamped.
/// [JP] 突発的な明るい外れ値の抑制。1サンプルが履歴輝度の何倍まで到達してよいか。
static const float REFLECTION_FIREFLY_MAX_RELATIVE_INTENSITY = 38.0;
static const float REFLECTION_FIREFLY_MIN_RELATIVE_SCALE = 2.0;
static const float REFLECTION_FIREFLY_RADIUS_SCALE = 0.1;

/// [EN] Cosine of the largest normal deviation a reprojected texel may have
///      before it counts as a disocclusion (89 degrees).
/// [JP] リプロジェクション先のテクセルがディスオクルージョン扱いになるまでに
///      許される法線のずれの余弦(89度)。
static const float REFLECTION_ALMOST_ZERO_ANGLE = 0.017452406;

/// [EN] Plane-distance disocclusion threshold, as a fraction of frustum size.
/// [JP] 平面距離によるディスオクルージョン閾値。視錐台サイズに対する割合。
static const float REFLECTION_DISOCCLUSION_THRESHOLD = 0.02;

/**
* [EN]
* Everything about the shaded pixel that every pass below needs: its view/world
* position, its shading frame, its roughness, and the two projection-derived
* scalars (world size of a pixel, world size of the frustum) that turn the
* pixel-space radii above into world-space kernel axes.
*
* ---------------------------------------------------------------------
*
* [JP]
* 下の各パスが共通して必要とするシェーディング対象ピクセルの情報一式:
* ビュー/ワールド座標、シェーディングフレーム、ラフネス、そして上のピクセル
* 単位の半径をワールド空間のカーネル軸へ変換するための射影由来の2つのスカラ
* (1ピクセルのワールドサイズ、視錐台のワールドサイズ)。
*/
struct ReflectionSurface
{
	float view_z_;
	float3 view_position_;
	float3 view_normal_;
	float3 world_position_;
	float3 normal_;
	float3 view_vector_;
	float roughness_;
	float unproject_;
	float frustum_size_;
	bool valid_;
};

/**
* [EN]
* Reconstructs the ReflectionSurface of one pixel from the G-Buffer. Returns
* valid_ = false for background (reverse-Z far plane), which every pass uses to
* bail out early rather than filtering pixels that have no surface.
*
* ---------------------------------------------------------------------
*
* [JP]
* G-Buffer から 1 ピクセルの ReflectionSurface を復元する。背景(reverse-Z の
* 遠平面)では valid_ = false を返す。各パスはこれを見て、面の無いピクセルを
* フィルタせず早期に抜ける。
*/
ReflectionSurface LoadReflectionSurface(SceneConstantBuffer scene, int2 pixel)
{
	ReflectionSurface surface;

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float4 gbuffer1 = normal_texture.Load(int3(pixel, 0));

	surface.view_position_ = DenoiserViewPosition(scene.inverse_projection_, uv, depth);
	surface.view_z_ = abs(surface.view_position_.z);
	surface.world_position_ = DenoiserWorldPosition(scene.inverse_view_projection_, uv, depth);
	surface.normal_ = OctNormalDecode(gbuffer1.rg);
	surface.view_normal_ = normalize(mul(surface.normal_, (float3x3)scene.view_));
	surface.view_vector_ = normalize(scene.camera_position_.xyz - surface.world_position_);
	surface.roughness_ = gbuffer1.b;
	surface.unproject_ = ReblurUnproject(scene.projection_[1][1], scene.screen_size_.y);
	surface.frustum_size_ = ReblurFrustumSize(surface.unproject_, scene.screen_size_, surface.view_z_);
	surface.valid_ = depth != 0.0;

	return surface;
}

/**
* [EN]
* Packs the per-pixel geometry TemporalAccumulation needs from the PREVIOUS
* frame - view depth, normal and roughness. The engine's G-Buffer is single
* buffered, so the disocclusion and virtual-motion tests would otherwise have
* nothing from last frame to compare against.
*
* ---------------------------------------------------------------------
*
* [JP]
* TemporalAccumulation が【前フレーム】から必要とするピクセルごとの幾何情報
* (ビュー深度・法線・ラフネス)を詰める。エンジンの G-Buffer は単一バッファ
* なので、これが無いとディスオクルージョン判定と仮想モーション判定が前フレームの
* 比較対象を持てない。
*/
float4 PackDepthNormalRoughness(float view_z, float3 normal, float roughness)
{
	return float4(view_z, OctNormalEncode(normal), roughness);
}

/**
* [EN]
* ReBLUR's cross-bilateral spatial filter, shared by the pre-pass, the blur and
* the post-blur (which differ only in radius/tolerance scale and in whether an
* accumulated history exists yet).
*
* The blur radius comes out of the hit distance rather than out of a constant:
* area_factor is how large the reflection's footprint is relative to the
* frustum, optionally attenuated by how many frames have already been
* accumulated, and the radius is its square root because the factor describes
* an AREA. It is then scaled by the specular magic curve, which drives it to
* zero as roughness approaches a mirror - a mirror reflection is not noisy and
* must not be touched.
*
* The kernel itself is 8 world-space Poisson taps on an ellipse whose axes are
* the tangent frame of the specular dominant direction, elongated by skew_factor
* at grazing angles where the lobe itself is elongated, then projected back to
* screen space. Every tap is then gated by the four ReBLUR weight families:
* plane distance (same surface), normal (same lobe orientation), roughness
* (same lobe width) and hit distance (reflecting something at a comparable
* depth).
*
* ---------------------------------------------------------------------
*
* [JP]
* ReBLUR のクロスバイラテラル空間フィルタ。プリパス/ブラー/ポストブラーで共有
* する(3者の違いは半径・許容度のスケールと、蓄積済み履歴が既に存在するか否か
* だけ)。
*
* ブラー半径は定数ではなくヒット距離から出る。area_factor は反射のフットプリント
* が視錐台に対してどれだけ大きいかで、必要なら既に蓄積したフレーム数で減衰させる。
* 半径がその平方根なのは、この係数が【面積】を表すため。さらにスペキュラ
* マジックカーブを掛けることで、ラフネスが鏡面に近づくにつれ半径が 0 へ向かう —
* 鏡面反射にはノイズが無く、触ってはいけない。
*
* カーネル本体はワールド空間の 8 点ポアソンタップで、その楕円の軸はスペキュラ
* 支配方向の接空間フレーム。ローブ自体が伸びるすれすれの角度では skew_factor で
* 引き伸ばし、最後に画面空間へ射影し直す。各タップは ReBLUR の 4 系統の重みで
* ゲートされる: 平面距離(同じ面か)、法線(同じローブ向きか)、ラフネス(同じ
* ローブ幅か)、ヒット距離(同程度の奥行きのものを映しているか)。
*/
float4 ReflectionSpatialFilter(SceneConstantBuffer scene, int2 pixel, ReflectionSurface surface, Texture2D<float4> source_texture, float4 center, float non_linear_accum_speed, float radius_scale, float fraction_scale, float max_blur_radius, bool clamp_to_lobe)
{
	int2 screen_max = int2(scene.screen_size_) - 1;

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];

	float4 dominant = ReblurSpecularDominantDirection(surface.normal_, surface.view_vector_, surface.roughness_);
	float normal_dot_dominant = abs(dot(surface.normal_, dominant.xyz));
	float smc = ReblurSpecMagicCurve(surface.roughness_, 0.5);

	float hit_distance_scale = ReblurHitDistanceNormalization(surface.view_z_, REFLECTION_HIT_DISTANCE_PARAMS, surface.roughness_);
	float hit_distance = center.w * hit_distance_scale;
	float hit_distance_factor = ReblurHitDistFactor(hit_distance, surface.frustum_size_);

	float area_factor = hit_distance_factor * non_linear_accum_speed;

	float blur_radius = radius_scale * sqrt(saturate(area_factor));
	blur_radius = saturate(blur_radius) * max_blur_radius * smc;
	blur_radius = max(blur_radius, REFLECTION_MIN_BLUR_RADIUS * smc);

	if (clamp_to_lobe)
	{
		/// [JP] プリパスは半径が大きいので、GGXローブの外へはみ出さないよう
		///      ヒット点でのローブ半径そのものでクランプする。
		float lobe_tan_half_angle = ReblurSpecularLobeTanHalfAngle(surface.roughness_, REBLUR_MAX_PERCENT_OF_LOBE_VOLUME_FOR_PRE_PASS);
		float world_lobe_radius = hit_distance * normal_dot_dominant * lobe_tan_half_angle;
		float lobe_radius = world_lobe_radius / ReblurPixelRadiusToWorld(surface.unproject_, 1.0, surface.view_z_ + hit_distance * dominant.w);

		blur_radius = min(blur_radius, lobe_radius);
	}

	if (blur_radius <= 0.0)
	{
		return center;
	}

	float2 geometry_weight_params = ReblurGeometryWeightParams(REFLECTION_PLANE_DISTANCE_SENSITIVITY, surface.frustum_size_, surface.view_position_, surface.view_normal_);
	float normal_weight_param = ReblurNormalWeightParam(non_linear_accum_speed, REFLECTION_LOBE_ANGLE_FRACTION, surface.roughness_) / fraction_scale;
	float2 roughness_weight_params = ReblurRoughnessWeightParams(surface.roughness_, REFLECTION_ROUGHNESS_FRACTION * fraction_scale);
	float2 hit_distance_weight_params = ReblurHitDistanceWeightParams(center.w, non_linear_accum_speed);
	float min_hit_distance_weight = REFLECTION_MIN_HIT_DISTANCE_WEIGHT * fraction_scale * smc * non_linear_accum_speed;

	/// [JP] ローブは視線がすれすれになるほど支配方向に沿って伸びる。カーネルの
	///      楕円をその方向へ引き伸ばし、直交方向を縮めることで、サンプルを
	///      「ローブが実際に広がっている側」へ集中させる。
	float bent_factor = sqrt(hit_distance_factor);
	float skew_factor = lerp(0.25 + 0.75 * surface.roughness_, 1.0, normal_dot_dominant);
	skew_factor = lerp(skew_factor, 1.0, non_linear_accum_speed);
	skew_factor = lerp(1.0, skew_factor, bent_factor);

	float3 bent_direction = normalize(lerp(surface.normal_, dominant.xyz, bent_factor));
	float3 bent_view_direction = normalize(mul(bent_direction, (float3x3)scene.view_));

	float world_radius = ReblurPixelRadiusToWorld(surface.unproject_, blur_radius, surface.view_z_);

	float2x3 kernel_basis = ReblurKernelBasis(bent_view_direction, surface.view_normal_);
	float3 kernel_tangent = kernel_basis[0] * world_radius * skew_factor;
	float3 kernel_bitangent = kernel_basis[1] * world_radius / skew_factor;

	ConstantBuffer<ReflectionRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.reflection_.ray_constant_index_];
	float rotation_angle = DenoiserBayer4x4(uint2(pixel), tuning.frame_index_) * 6.28318530718;
	float2 rotator = float2(cos(rotation_angle), sin(rotation_angle));

	float4 result = center;
	float weight_sum = 1.0;

	[unroll]
	for (uint tap = 0; tap < 8; ++tap)
	{
		float3 offset = REBLUR_POISSON_SAMPLES[tap];

		float2 tap_uv = ReblurKernelSampleUv(scene.projection_, offset, surface.view_position_, kernel_tangent, kernel_bitangent, rotator);

		/// [JP] 画面外へ出たタップはクランプせずミラーで折り返す。クランプだと
		///      複数タップが同じ端テクセルへ潰れて実質サンプル数が落ちる。
		float2 mirror_uv = ReblurMirrorUv(tap_uv);
		float weight = any(tap_uv != mirror_uv) ? 1.0 : ReblurGaussianWeight(offset.z);

		int2 tap_pixel = clamp(int2(mirror_uv * scene.screen_size_), int2(0, 0), screen_max);

		float tap_depth = depth_texture.Load(int3(tap_pixel, 0));
		if (tap_depth == 0.0)
		{
			continue;
		}

		float2 tap_center_uv = (float2(tap_pixel) + 0.5) * scene.inverse_screen_size_;
		float3 tap_view_position = DenoiserViewPosition(scene.inverse_projection_, tap_center_uv, tap_depth);

		float4 tap_gbuffer1 = normal_texture.Load(int3(tap_pixel, 0));
		float3 tap_normal = OctNormalDecode(tap_gbuffer1.rg);
		float tap_roughness = tap_gbuffer1.b;

		float angle = acos(saturate(dot(surface.normal_, tap_normal)));
		weight *= ReblurComputeWeight(angle, normal_weight_param, 0.0);
		weight *= ReblurComputeWeight(tap_roughness, roughness_weight_params.x, roughness_weight_params.y);
		weight *= ReblurComputeWeight(dot(surface.view_normal_, tap_view_position), geometry_weight_params.x, geometry_weight_params.y);

		float4 tap_value = source_texture.Load(int3(tap_pixel, 0));

		weight *= min_hit_distance_weight + ReblurComputeExponentialWeight(tap_value.w, hit_distance_weight_params.x, hit_distance_weight_params.y);

		result += tap_value * weight;
		weight_sum += weight;
	}

	result /= weight_sum;

	/// [JP] ヒット距離は空間フィルタの結果を採らず中心の値を保つ。ここで平均して
	///      しまうと、次のパスが読む半径が「近傍の平均距離」由来になり、自分の
	///      出力で自分の半径を決めるフィードバックループでバイアスが積み上がる。
	result.w = center.w;

	return result;
}

/**
* [EN]
* Pre-pass: one spatial reuse pass BEFORE any temporal accumulation exists.
* Probabilistic 1spp specular is sparse enough that a lone bright sample can
* survive reprojection as a bright blob, so this pass exists purely to make the
* signal well-defined enough for TemporalAccumulation to track it. Its blur
* radius uses the raw hit distance factor with no accumulation attenuation
* (there is no history yet), and is clamped to stay inside the GGX lobe.
*
* ---------------------------------------------------------------------
*
* [JP]
* プリパス: 時間的蓄積がまだ存在しない段階で行う 1 回の空間再利用パス。確率的な
* 1spp スペキュラはサンプルが疎で、孤立した明るいサンプルがリプロジェクションを
* 生き延びて明るい塊として残りうる。このパスの目的はひとえに、
* TemporalAccumulation が追従できる程度に信号を「まともな形」にすること。
* ブラー半径には蓄積による減衰を掛けない生のヒット距離係数を使い(履歴がまだ
* 無いため)、GGXローブの内側に収まるようクランプする。
*/
[numthreads(8, 8, 1)]
void PrePass(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	int2 pixel = int2(dtid.xy);

	Texture2D<float4> raw_reflection = ResourceDescriptorHeap[structured_indices.reflection_.output_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.reflection_.scratch0_uav_index_];

	ReflectionSurface surface = LoadReflectionSurface(scene, pixel);

	if (!surface.valid_)
	{
		dest[pixel] = float4(0, 0, 0, 0);
		return;
	}

	float4 center = raw_reflection.Load(int3(pixel, 0));

	dest[pixel] = ReflectionSpatialFilter(scene, pixel, surface, raw_reflection, center, 1.0, REFLECTION_PREPASS_RADIUS_SCALE, REFLECTION_PREPASS_FRACTION_SCALE, REFLECTION_PREPASS_BLUR_RADIUS, true);
}

/**
* [EN]
* Temporal accumulation with dual (surface + virtual) reprojection.
*
* Surface motion is the ordinary G-Buffer velocity reprojection, validated
* against last frame's depth/normal with a 2x2 plane-distance and normal
* disocclusion test. Virtual motion places a point behind the surface at the
* image position the thin-lens equation gives for a mirror of the locally
* estimated curvature, and reprojects THAT - which is where the reflected
* content actually was last frame. The two are blended by which of them can
* justify more accumulated frames, so a flat mirror follows virtual motion
* almost entirely while a bumpy or very rough surface falls back to surface
* motion, where virtual motion is unreliable.
*
* The accumulated frame count itself is the state that everything downstream
* reads: the blend factor is 1 / (1 + frames), and the spatial passes shrink
* their radius by the same quantity, so a converged pixel simultaneously stops
* taking new samples and stops being blurred.
*
* ---------------------------------------------------------------------
*
* [JP]
* 面モーションと仮想モーションの二重リプロジェクションによる時間的蓄積。
*
* 面モーションは通常の G-Buffer 速度によるリプロジェクションで、前フレームの
* 深度/法線に対する 2x2 の平面距離+法線ディスオクルージョン判定で検証する。
* 仮想モーションは、局所推定した曲率の鏡に対する薄レンズの式が与える結像位置へ
* 面の裏側に点を置き、【そちら】をリプロジェクションする — 映っている内容が
* 前フレームに実際に在った場所はそこだから。両者は「どちらがより多くの蓄積
* フレーム数を正当化できるか」でブレンドするので、平らな鏡ではほぼ仮想モーション
* に従い、凹凸のある面や非常に粗い面では仮想モーションが当てにならないぶん面
* モーションへ退く。
*
* 蓄積フレーム数そのものが、以降の全パスが読む状態になる。ブレンド係数は
* 1 / (1 + フレーム数) であり、空間パスも同じ量で半径を縮める — つまり収束した
* ピクセルは新しいサンプルを取り込むのをやめると同時に、ぼかされるのもやめる。
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

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.reflection_.scratch0_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.reflection_.scratch1_uav_index_];
	RWTexture2D<float> accum_speed_output = ResourceDescriptorHeap[constant_indices.reflection_.accum_speed_uav_index_];
	RWTexture2D<float4> depth_normal_output = ResourceDescriptorHeap[constant_indices.reflection_.depth_normal_uav_index_];

	ReflectionSurface surface = LoadReflectionSurface(scene, pixel);

	if (!surface.valid_)
	{
		dest[pixel] = float4(0, 0, 0, 0);
		accum_speed_output[pixel] = 0.0;
		depth_normal_output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	depth_normal_output[pixel] = PackDepthNormalRoughness(surface.view_z_, surface.normal_, surface.roughness_);

	float4 current = source.Load(int3(pixel, 0));

	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;

	Texture2D<float2> velocity_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_2_];
	float2 velocity = velocity_texture.Load(int3(pixel, 0));
	float2 surface_uv = DenoiserPreviousUv(uv, velocity);

	Texture2D<float4> history_texture = ResourceDescriptorHeap[constant_indices.reflection_.history_srv_index_];
	Texture2D<float> history_accum_speed = ResourceDescriptorHeap[constant_indices.reflection_.accum_speed_history_srv_index_];
	Texture2D<float4> history_depth_normal = ResourceDescriptorHeap[constant_indices.reflection_.depth_normal_history_srv_index_];

	float pixel_size = ReblurPixelRadiusToWorld(surface.unproject_, 1.0, surface.view_z_);
	float normal_dot_view = abs(dot(surface.normal_, surface.view_vector_));
	float disocclusion_threshold = REFLECTION_DISOCCLUSION_THRESHOLD * surface.frustum_size_ / max(0.05, normal_dot_view);

	/// [JP] 履歴に保存されているビュー深度は【前フレームのカメラ】から測った値
	///      なので、比較相手も同じ基準へ揃える必要がある。今のカメラから測った
	///      surface.view_z_ と直接比べると、カメラが移動しただけで、静止した面を
	///      正しく再投影できている場合でも移動量ぶん食い違い、毎フレーム履歴を
	///      捨ててしまう(回転ではビュー深度が変わらないので露見せず、移動時だけ
	///      ノイズへ戻る)。
	///
	///      LH 透視射影では clip.w がそのままビュー深度になるので、今の world
	///      座標を前フレームの view-projection へ通した w が、そのまま
	///      「前フレームのカメラから見たこの点の深度」になる。前フレームのビュー
	///      行列を別途持たなくても済む。
	float previous_view_z = mul(float4(surface.world_position_, 1.0), scene.previous_view_projection_).w;

	/// [JP] 面モーション側のディスオクルージョン判定と蓄積速度の取得。2x2 の
	///      各テクセルを「今の接平面からの距離」と「法線のずれ」で個別に検証し、
	///      生き残ったものだけでバイリニア重みを組み直す。
	float surface_accum_speed = 0.0;
	float surface_footprint = 0.0;
	{
		float2 previous_position = surface_uv * scene.screen_size_ - 0.5;
		int2 previous_base = int2(floor(previous_position));
		float2 previous_fraction = previous_position - float2(previous_base);

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
			if (tap_depth_normal.x <= 0.0)
			{
				continue;
			}

			float3 tap_normal = OctNormalDecode(tap_depth_normal.yz);
			if (dot(tap_normal, surface.normal_) < REFLECTION_ALMOST_ZERO_ANGLE)
			{
				continue;
			}

			if (abs(tap_depth_normal.x - previous_view_z) > disocclusion_threshold)
			{
				continue;
			}

			surface_accum_speed += history_accum_speed.Load(int3(tap_pixel, 0)) * bilinear_weights[tap];
			weight_sum += bilinear_weights[tap];
		}

		if (weight_sum >= 0.01)
		{
			/// [JP] +1 は「前フレームまでに積んだ数 → 今フレーム開始時点で
			///      積み終えている数」への繰り上げ。このテクスチャは書いた
			///      フレームより【前】までの数を保持する規約なので、無効な
			///      リプロジェクションでは 0 のまま(履歴ゼロ)になる。
			surface_accum_speed = surface_accum_speed / weight_sum + 1.0;
			surface_footprint = sqrt(saturate(weight_sum));
		}
	}

	/// [JP] 曲率推定。右隣/下隣のピクセルの法線と、その画素を中心ピクセルの接平面
	///      へ落とした点を使い、法線の変化率を辺の長さで割る。平面なら 0 になり、
	///      その場合 GetXvirtual は「ヒット距離ぶん奥」に素直な虚像を置く。
	float curvature = 0.0;
	{
		int2 right_pixel = clamp(pixel + int2(1, 0), int2(0, 0), screen_max);
		int2 down_pixel = clamp(pixel + int2(0, 1), int2(0, 0), screen_max);

		Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
		float3 right_normal = OctNormalDecode(normal_texture.Load(int3(right_pixel, 0)).rg);
		float3 down_normal = OctNormalDecode(normal_texture.Load(int3(down_pixel, 0)).rg);

		float2 right_uv = (float2(right_pixel) + 0.5) * scene.inverse_screen_size_;
		float2 down_uv = (float2(down_pixel) + 0.5) * scene.inverse_screen_size_;

		float3 right_ray = normalize(DenoiserWorldPosition(scene.inverse_view_projection_, right_uv, 0.5) - scene.camera_position_.xyz);
		float3 down_ray = normalize(DenoiserWorldPosition(scene.inverse_view_projection_, down_uv, 0.5) - scene.camera_position_.xyz);

		/// [JP] 隣接ピクセルの深度そのものではなく、その視線と中心の接平面との
		///      交点を使う。実深度だと段差やノイズを曲率として拾ってしまい、
		///      仮想位置が面の中へ引き込まれて逆に遅延が出る。
		float3 right_position = scene.camera_position_.xyz + right_ray * dot(surface.world_position_ - scene.camera_position_.xyz, surface.normal_) / max(dot(surface.normal_, right_ray), 1e-4);
		float3 down_position = scene.camera_position_.xyz + down_ray * dot(surface.world_position_ - scene.camera_position_.xyz, surface.normal_) / max(dot(surface.normal_, down_ray), 1e-4);

		float3 edge_position = (right_position + down_position) * 0.5;
		float3 edge_normal = normalize(right_normal + down_normal);

		float3 edge = edge_position - surface.world_position_;
		float edge_length_squared = dot(edge, edge);

		curvature = edge_length_squared > 1e-9 ? dot(edge_normal - surface.normal_, edge) / edge_length_squared : 0.0;
	}

	float hit_distance_scale = ReblurHitDistanceNormalization(surface.view_z_, REFLECTION_HIT_DISTANCE_PARAMS, surface.roughness_);
	float hit_distance_for_tracking = current.w * hit_distance_scale;

	/// [JP] 前フレームのワールド座標は、速度が画面空間のみでこのピクセルの前
	///      フレーム深度が残っていないため厳密には復元できない。ReBLUR が
	///      これを使うのは「虚像が面に焦点を結んだときに面モーション側へ寄せる」
	///      アンカーとしてだけなので、静止近似(現在位置)で足りる。
	float3 virtual_position = ReblurVirtualPosition(hit_distance_for_tracking, curvature, surface.world_position_, surface.world_position_, surface.normal_, surface.view_vector_, surface.roughness_);

	float4 virtual_clip = mul(float4(virtual_position, 1.0), scene.previous_view_projection_);
	float2 virtual_ndc = virtual_clip.xy / max(virtual_clip.w, 1e-6);
	float2 virtual_uv = float2(virtual_ndc.x * 0.5 + 0.5, 0.5 - virtual_ndc.y * 0.5);

	/// [JP] 仮想モーション側の信頼度。ラフネスと法線が前フレームと合っていなければ
	///      「同じローブが映っている」とは言えないので履歴を認めない。
	float virtual_accum_speed = 0.0;
	float virtual_confidence = 0.0;
	{
		int2 virtual_pixel = int2(virtual_uv * scene.screen_size_);

		if (all(virtual_pixel >= int2(0, 0)) && all(virtual_pixel <= screen_max))
		{
			float4 virtual_depth_normal = history_depth_normal.Load(int3(virtual_pixel, 0));

			if (virtual_depth_normal.x > 0.0)
			{
				float3 virtual_normal = OctNormalDecode(virtual_depth_normal.yz);
				float2 roughness_weight_params = ReblurRoughnessWeightParams(surface.roughness_, REFLECTION_ROUGHNESS_FRACTION);

				float roughness_weight = ReblurComputeWeight(virtual_depth_normal.w, roughness_weight_params.x, roughness_weight_params.y);
				float normal_weight = dot(virtual_normal, surface.normal_) > REFLECTION_ALMOST_ZERO_ANGLE ? 1.0 : 0.0;

				virtual_confidence = roughness_weight * normal_weight;

				/// [JP] 仮想モーションのディスオクルージョン判定(NRD の
				///      vmbPlaneDist 相当)。仮想UVは「同じ反射面の上」に着地して
				///      いなければならない。着地先が別の面なら、そこの履歴は
				///      この反射とは無関係でありゴーストの直接の原因になる。
				///      法線とラフネスの一致だけでは、平行な別の面(床と机の天板、
				///      同じ壁の別区画)を弾けないため、幾何的な距離で判定する。
				///
				///      カメラを左右上下へ振ると視差が大きく効くので、仮想UVの
				///      ずれがそのまま「別の面への着地」になる。前後移動では
				///      ずれが視線方向に潰れて表面化しにくい — 上下左右だけ
				///      ゴーストが残っていたのはこのため。
				///
				///      比較は前フレームのビュー空間(カメラ相対)で行う。前フレームの
				///      ビュー行列は、行ベクトル規約で VP·P⁻¹ = V となることから
				///      追加の定数バッファ無しで復元できる。
				float4x4 previous_view = mul(scene.previous_view_projection_, scene.inverse_projection_);

				float3 previous_view_normal = normalize(mul(surface.normal_, (float3x3)previous_view));
				float3 previous_view_surface = mul(float4(surface.world_position_, 1.0), previous_view).xyz;

				/// [JP] 着地先ピクセルが前フレームに持っていたビュー座標を、保存済み
				///      ビュー深度と射影パラメータから復元する。
				float2 virtual_reconstruct_ndc = float2(virtual_uv.x * 2.0 - 1.0, 1.0 - virtual_uv.y * 2.0);
				float3 previous_view_tap = float3(
					virtual_reconstruct_ndc.x * virtual_depth_normal.x / scene.projection_[0][0],
					virtual_reconstruct_ndc.y * virtual_depth_normal.x / scene.projection_[1][1],
					virtual_depth_normal.x);

				float virtual_plane_distance = abs(dot(previous_view_normal, previous_view_tap - previous_view_surface));

				/// [JP] しきい値の NoV は乗算。すれすれの角度ほど厳しくするのが
				///      正しい(除算にすると逆に緩み、まれな明るいサンプルが
				///      引き伸ばされる)。
				float virtual_plane_threshold = REFLECTION_DISOCCLUSION_THRESHOLD * surface.frustum_size_ * lerp(0.1, 1.0, normal_dot_view);

				virtual_confidence *= virtual_plane_distance <= virtual_plane_threshold ? 1.0 : 0.0;

				/// [JP] 視差テスト。仮想UVは 1spp のノイジーなヒット距離から
				///      計算されるので、その距離が少しずれるだけで着地点が動く。
				///      カメラがその場で回るだけなら視差が無く着地点も動かない
				///      ため誤差は出ないが、カメラが【移動】すると視差が効き、
				///      ずれがそのまま「別の場所の履歴を引く」= ゴーストになる。
				///      そこで前フレームの蓄積ヒット距離で同じ仮想位置を引き直し、
				///      両者の着地点の差がローブの広がり(ピクセル)を超えたら
				///      仮想モーションは追跡できていないと判断して信用を落とす。
				float previous_hit_distance = history_texture.SampleLevel(sampler_linear_clamp, virtual_uv, 0).w * hit_distance_scale;

				float3 previous_virtual_position = ReblurVirtualPosition(previous_hit_distance, curvature, surface.world_position_, surface.world_position_, surface.normal_, surface.view_vector_, surface.roughness_);

				float4 previous_virtual_clip = mul(float4(previous_virtual_position, 1.0), scene.previous_view_projection_);
				float2 previous_virtual_ndc = previous_virtual_clip.xy / max(previous_virtual_clip.w, 1e-6);
				float2 previous_virtual_uv = float2(previous_virtual_ndc.x * 0.5 + 0.5, 0.5 - previous_virtual_ndc.y * 0.5);

				/// [JP] 許容ずれ幅は、虚像の距離におけるローブ半径をピクセルへ
				///      直したもの。粗い面ほどローブが広く多少のずれは無害なので
				///      緩み、鏡面ほど厳しくなる。
				float virtual_distance = length(virtual_position - scene.camera_position_.xyz);
				float lobe_tan_half_angle = ReblurSpecularLobeTanHalfAngle(surface.roughness_, REBLUR_MAX_PERCENT_OF_LOBE_VOLUME);
				float lobe_radius_in_pixels = min(hit_distance_for_tracking, previous_hit_distance) * lobe_tan_half_angle / max(ReblurPixelRadiusToWorld(surface.unproject_, 1.0, virtual_distance), REBLUR_EPSILON);

				float allowed_drift = max(0.5 * lobe_radius_in_pixels, 0.1 * surface.roughness_);
				float drift = length((previous_virtual_uv - virtual_uv) * scene.screen_size_);

				virtual_confidence *= saturate(1.0 - drift / max(allowed_drift, REBLUR_EPSILON));

				virtual_accum_speed = (history_accum_speed.Load(int3(virtual_pixel, 0)) + 1.0) * virtual_confidence;
			}
		}
	}

	surface_accum_speed *= lerp(surface_footprint, 1.0, 1.0 / (1.0 + surface_accum_speed));

	/// [JP] どちらのリプロジェクションを採るかは「認められた履歴の長さの差」で
	///      決める。仮想側が面側と同等以上に履歴を保てているなら完全に仮想側へ、
	///      劣るぶんだけ面側へ引き戻す。
	float virtual_history_amount = saturate(1.0 + (virtual_accum_speed - surface_accum_speed) / (1.0 + 0.5 * max(virtual_accum_speed, surface_accum_speed)));

	float2 history_uv = lerp(surface_uv, virtual_uv, virtual_history_amount);
	bool history_in_screen = all(history_uv >= 0.0) && all(history_uv <= 1.0);

	float accum_speed = lerp(surface_accum_speed, virtual_accum_speed, virtual_history_amount);
	accum_speed = history_in_screen ? min(accum_speed, REFLECTION_MAX_ACCUM_FRAME_NUM) : 0.0;

	float4 history = history_in_screen ? history_texture.SampleLevel(sampler_linear_clamp, history_uv, 0) : float4(0, 0, 0, 0);
	history.rgb = max(history.rgb, 0.0);
	history.w = saturate(history.w);

	float non_linear_accum_speed = 1.0 / (1.0 + accum_speed);

	/// [JP] ヒット距離だけは色より速く追従させる。ヒット距離が履歴に引きずられると
	///      空間パスの半径が実際のレイより遅れて動き、色に階段状のバンディングが出る。
	float hit_distance_min_speed = 1.0 / (1.0 + 0.5 * ReblurSpecMagicCurve(surface.roughness_, 0.25) * REFLECTION_MAX_ACCUM_FRAME_NUM);

	float4 result;
	result.rgb = lerp(history.rgb, current.rgb, non_linear_accum_speed);
	result.w = lerp(history.w, current.w, max(non_linear_accum_speed, hit_distance_min_speed));

	/// [JP] 短期履歴の更新と、それによる長期履歴のクランプ(ReBLUR のアンチラグ)。
	///      箱の中心は 6 フレーム履歴、幅は今フレームの 3x3 ルミナンス標準偏差。
	///      色相を壊さないよう、NRD と同じくルミナンスだけをクランプして RGB は
	///      その比率でスケールする。
	{
		float current_luminance = dot(current.rgb, float3(0.2126, 0.7152, 0.0722));

		float luminance_sum = 0.0;
		float luminance_squared_sum = 0.0;

		[unroll]
		for (int dy = -1; dy <= 1; ++dy)
		{
			[unroll]
			for (int dx = -1; dx <= 1; ++dx)
			{
				int2 tap_pixel = clamp(pixel + int2(dx, dy), int2(0, 0), screen_max);
				float tap_luminance = dot(source.Load(int3(tap_pixel, 0)).rgb, float3(0.2126, 0.7152, 0.0722));
				luminance_sum += tap_luminance;
				luminance_squared_sum += tap_luminance * tap_luminance;
			}
		}

		float neighborhood_mean = luminance_sum / 9.0;
		float neighborhood_sigma = sqrt(max(luminance_squared_sum / 9.0 - neighborhood_mean * neighborhood_mean, 0.0));

		Texture2D<float> history_fast = ResourceDescriptorHeap[constant_indices.reflection_.fast_history_history_srv_index_];
		RWTexture2D<float> fast_output = ResourceDescriptorHeap[constant_indices.reflection_.fast_history_uav_index_];

		float fast_history_luminance = history_in_screen ? history_fast.SampleLevel(sampler_linear_clamp, history_uv, 0) : current_luminance;

		float fast_non_linear_accum_speed = 1.0 / (1.0 + min(accum_speed, REFLECTION_MAX_FAST_ACCUM_FRAME_NUM));
		float fast_luminance = lerp(max(fast_history_luminance, 0.0), current_luminance, fast_non_linear_accum_speed);

		fast_output[pixel] = fast_luminance;

		float clamp_radius = REFLECTION_FAST_HISTORY_CLAMP_SIGMA * neighborhood_sigma;
		float result_luminance = dot(result.rgb, float3(0.2126, 0.7152, 0.0722));
		float clamped_luminance = clamp(result_luminance, fast_luminance - clamp_radius, fast_luminance + clamp_radius);

		result.rgb *= (clamped_luminance + 1e-6) / (result_luminance + 1e-6);
	}

	/// [JP] ファイアフライ抑制。履歴が長いほど強く効かせる — 収束したはずの
	///      ピクセルに突然現れる 1 サンプルは、ほぼ確実に外れ値であるため。
	{
		float max_relative_intensity = REFLECTION_FIREFLY_MIN_RELATIVE_SCALE + REFLECTION_FIREFLY_MAX_RELATIVE_INTENSITY / (accum_speed + 1.0);

		float antifirefly_factor = accum_speed * REFLECTION_MAX_BLUR_RADIUS * REFLECTION_FIREFLY_RADIUS_SCALE;
		antifirefly_factor /= 1.0 + antifirefly_factor;

		float result_luminance = dot(result.rgb, float3(0.2126, 0.7152, 0.0722));
		float history_luminance = dot(history.rgb, float3(0.2126, 0.7152, 0.0722));

		float clamped_luminance = lerp(result_luminance, min(result_luminance, history_luminance * max_relative_intensity), antifirefly_factor);
		result.rgb *= (clamped_luminance + 1e-6) / (result_luminance + 1e-6);
	}

	dest[pixel] = result;

	/// [JP] 書くのは「今フレーム開始時点で積み終えている数」そのもので、+1 した値
	///      ではない。この値は次フレームのリプロジェクションが読む(そこで +1
	///      される)と同時に、【今フレームの】HistoryFix/Blur/PostBlur も読む —
	///      +1 して書くと空間パスが実際より 1 フレーム収束が進んだと誤認し、
	///      ディスオクルージョン直後の最も強くぼかすべきピクセルで半径が半分に
	///      なってしまう。
	accum_speed_output[pixel] = accum_speed;
}

/**
* [EN]
* History fix: spatial reconstruction for pixels that have almost no accumulated
* history (fresh disocclusions, camera cuts). Those pixels are effectively still
* 1spp, and no amount of temporal blending will help them this frame, so they
* borrow from converged neighbors instead - via a sparse 5x5 kernel with a very
* large stride, because the nearest converged neighbor of a disocclusion is by
* definition outside the disoccluded region. Each tap is weighted by how much
* history IT has, so the reconstruction is pulled toward the converged pixels
* rather than averaging equally with other unconverged ones. Pixels that already
* have enough history pass through untouched.
*
* ---------------------------------------------------------------------
*
* [JP]
* ヒストリフィックス: 蓄積履歴をほとんど持たないピクセル(直後のディスオクルー
* ジョン、カメラカット)の空間的再構成。それらは実質まだ 1spp であり、今フレーム
* にどれだけ時間ブレンドしても救われないので、代わりに収束済みの近傍から借りて
* くる。カーネルは間隔を非常に大きく取った疎な 5x5 — ディスオクルージョン領域の
* 最寄りの収束済み近傍は、定義上その領域の外にあるため。各タップは「そのタップ
* 自身がどれだけ履歴を持つか」で重み付けするので、再構成は未収束同士の平均では
* なく収束済みピクセル側へ引っ張られる。既に履歴が足りているピクセルは素通し。
*/
[numthreads(8, 8, 1)]
void HistoryFix(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	int2 pixel = int2(dtid.xy);
	int2 screen_max = int2(scene.screen_size_) - 1;

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.reflection_.scratch1_srv_index_];
	Texture2D<float> accum_speed_texture = ResourceDescriptorHeap[constant_indices.reflection_.accum_speed_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.reflection_.scratch0_uav_index_];

	ReflectionSurface surface = LoadReflectionSurface(scene, pixel);

	float4 center = source.Load(int3(pixel, 0));

	if (!surface.valid_)
	{
		dest[pixel] = center;
		return;
	}

	float frame_num = accum_speed_texture.Load(int3(pixel, 0));

	if (frame_num >= REFLECTION_HISTORY_FIX_FRAME_NUM)
	{
		dest[pixel] = center;
		return;
	}

	float non_linear_accum_speed = 1.0 / (1.0 + frame_num);

	float hit_distance_scale = ReblurHitDistanceNormalization(surface.view_z_, REFLECTION_HIT_DISTANCE_PARAMS, surface.roughness_);
	float hit_distance_factor = ReblurHitDistFactor(center.w * hit_distance_scale, surface.frustum_size_);
	float smc = ReblurSpecMagicCurve(surface.roughness_, 0.25);

	/// [JP] 間隔は「履歴がどれだけ足りないか」で決まる。足りないほど遠くまで
	///      探しに行く。ヒット距離が近い(接触反射)場合とラフネスが低い場合は
	///      詰める — そこは遠くから借りると明確に間違うため。
	float stride = REFLECTION_HISTORY_FIX_STRIDE * saturate(1.0 - frame_num / REFLECTION_HISTORY_FIX_FRAME_NUM);
	stride *= lerp(0.25 + 0.75 * sqrt(saturate(hit_distance_factor)), 1.0, non_linear_accum_speed);
	stride *= lerp(0.25, 1.0, smc);
	stride = round(stride);

	if (stride <= 0.0)
	{
		dest[pixel] = center;
		return;
	}

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];

	float normal_weight_param = ReblurNormalWeightParam(non_linear_accum_speed, REFLECTION_LOBE_ANGLE_FRACTION, surface.roughness_);
	float2 geometry_weight_params = ReblurGeometryWeightParams(REFLECTION_PLANE_DISTANCE_SENSITIVITY, surface.frustum_size_, surface.view_position_, surface.view_normal_);
	float2 roughness_weight_params = ReblurRoughnessWeightParams(surface.roughness_, REFLECTION_ROUGHNESS_FRACTION);
	float2 hit_distance_weight_params = ReblurHitDistanceWeightParams(center.w, non_linear_accum_speed);

	float weight_sum = 1.0 + frame_num;
	float4 result = center * weight_sum;

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

			/// [JP] 角のタップは落とす。同じ半径で十字/菱形にした方が、疎な
			///      カーネルではサンプルの偏りが小さい。
			if (abs(dx) + abs(dy) == 4)
			{
				continue;
			}

			int2 tap_pixel = clamp(pixel + int2(dx, dy) * int(stride), int2(0, 0), screen_max);

			float tap_depth = depth_texture.Load(int3(tap_pixel, 0));
			if (tap_depth == 0.0)
			{
				continue;
			}

			float2 tap_uv = (float2(tap_pixel) + 0.5) * scene.inverse_screen_size_;
			float3 tap_view_position = DenoiserViewPosition(scene.inverse_projection_, tap_uv, tap_depth);

			float4 tap_gbuffer1 = normal_texture.Load(int3(tap_pixel, 0));
			float3 tap_normal = OctNormalDecode(tap_gbuffer1.rg);

			float angle = acos(saturate(dot(surface.normal_, tap_normal)));

			float weight = ReblurComputeExponentialWeight(angle, normal_weight_param, 0.0);
			weight *= ReblurComputeWeight(tap_gbuffer1.b, roughness_weight_params.x, roughness_weight_params.y);
			weight *= ReblurComputeWeight(dot(surface.view_normal_, tap_view_position), geometry_weight_params.x, geometry_weight_params.y);

			/// [JP] タップ自身の履歴長で重み付け。これが「収束済みの近傍から
			///      借りる」を実現している本体。
			weight *= 1.0 + accum_speed_texture.Load(int3(tap_pixel, 0));

			float4 tap_value = source.Load(int3(tap_pixel, 0));
			weight *= ReblurComputeExponentialWeight(tap_value.w, hit_distance_weight_params.x, hit_distance_weight_params.y);

			result += tap_value * weight;
			weight_sum += weight;
		}
	}

	dest[pixel] = result / weight_sum;
}

/**
* [EN]
* Main spatial pass. Same kernel as the pre-pass, but the radius is now
* attenuated by the accumulated frame count, so the filter narrows as the pixel
* converges.
*
* ---------------------------------------------------------------------
*
* [JP]
* 主空間パス。カーネルはプリパスと同じだが、半径が蓄積フレーム数で減衰する —
* ピクセルが収束するにつれてフィルタが狭まる。
*/
[numthreads(8, 8, 1)]
void Blur(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	int2 pixel = int2(dtid.xy);

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.reflection_.scratch0_srv_index_];
	Texture2D<float> accum_speed_texture = ResourceDescriptorHeap[constant_indices.reflection_.accum_speed_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.reflection_.scratch1_uav_index_];

	ReflectionSurface surface = LoadReflectionSurface(scene, pixel);

	float4 center = source.Load(int3(pixel, 0));

	if (!surface.valid_)
	{
		dest[pixel] = float4(0, 0, 0, 0);
		return;
	}

	float non_linear_accum_speed = 1.0 / (1.0 + accum_speed_texture.Load(int3(pixel, 0)));

	dest[pixel] = ReflectionSpatialFilter(scene, pixel, surface, source, center, non_linear_accum_speed, REFLECTION_BLUR_RADIUS_SCALE, REFLECTION_BLUR_FRACTION_SCALE, REFLECTION_MAX_BLUR_RADIUS, false);
}

/**
* [EN]
* Post-blur: the last spatial pass, reaching twice as far as Blur with half its
* weight tolerance. The wider reach mops up the low-frequency residual the
* previous pass could not span; the tighter tolerance is what keeps that extra
* reach from bleeding across surfaces. Its output is both what
* DeferredLightingPS.hlsl samples and next frame's temporal history, so the
* alpha channel keeps carrying the accumulated hit distance.
*
* ---------------------------------------------------------------------
*
* [JP]
* ポストブラー: 最後の空間パス。Blur の 2 倍の距離まで届き、重みの許容度は半分。
* 届く範囲が広いぶん前パスでは跨げなかった低周波の残りを均し、許容度が厳しい
* ぶんその広さが面をまたいで滲むのを防ぐ。この出力は DeferredLightingPS.hlsl が
* サンプルするものであると同時に次フレームの時間的履歴でもあるため、アルファ
* チャンネルは蓄積済みヒット距離を保持し続ける。
*/
[numthreads(8, 8, 1)]
void PostBlur(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	int2 pixel = int2(dtid.xy);

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.reflection_.scratch1_srv_index_];
	Texture2D<float> accum_speed_texture = ResourceDescriptorHeap[constant_indices.reflection_.accum_speed_srv_index_];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[constant_indices.reflection_.accumulated_uav_index_];

	ReflectionSurface surface = LoadReflectionSurface(scene, pixel);

	float4 center = source.Load(int3(pixel, 0));

	if (!surface.valid_)
	{
		dest[pixel] = float4(0, 0, 0, 0);
		return;
	}

	float non_linear_accum_speed = 1.0 / (1.0 + accum_speed_texture.Load(int3(pixel, 0)));

	dest[pixel] = ReflectionSpatialFilter(scene, pixel, surface, source, center, non_linear_accum_speed, REFLECTION_POST_BLUR_RADIUS_SCALE, REFLECTION_POST_BLUR_FRACTION_SCALE, REFLECTION_MAX_BLUR_RADIUS, false);
}

#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Noise.hlsli"
#include "../../Shader/Material.hlsli"
#include "../../Light/Cluster.hlsli"
#include "../Reflection/Reflection.hlsli"
#include "Shadow.hlsli"

/**
* [EN]
* Reference:
* - https://cs.dartmouth.edu/~wjarosz/publications/bitterli20spatiotemporal.html
*   (Bitterli et al., "Spatiotemporal reservoir resampling for real-time ray
*   tracing with dynamic direct lighting", SIGGRAPH 2020 - the RIS/ReSTIR DI
*   selection scheme this pass's punctual-light picker follows, minus its
*   temporal/spatial reuse passes, which are not implemented here.)
* - https://intro-to-restir.cwyman.org/presentations/2023ReSTIR_Course_Notes.pdf
*   (Wyman et al., ReSTIR course notes - target function / RIS weighting
*   background.)
*
* Stochastic ray-traced shadow + ReSTIR DI pass (inline RayQuery). One thread
* per screen pixel, one shadow ray per pixel per frame:
*   - Directional light: always tested (cheap, single dominant light), ray
*     jittered within a cone sized by sun_angular_radius_ for a soft penumbra.
*     Output stays a scalar visibility, same as before.
*   - Point/Spot/Rect lights: picks ONE light per pixel per frame via
*     weighted reservoir sampling (no per-light cost growth) — the weight is
*     each light's actual contribution to this pixel (intensity * distance
*     attenuation * spot cone fade, zero if behind the surface, outside the
*     spot cone, or the wrong side of a rect light), so lights that don't
*     actually illuminate the pixel are never selected and never waste the
*     one shadow ray. Jitters the ray toward a random point on/near the
*     picked light's surface (physical size -> free soft shadow for rect
*     lights; a fixed world-space radius for point/spot). Unlike the
*     directional term, this is a full ReSTIR DI-style estimator: it resolves
*     this pixel's G-Buffer material (Shader/Material.hlsli's
*     ResolveGBufferMaterial - the SAME function Model/Opaque/
*     DeferredLightingPS.hlsl uses, so the specular response matches),
*     evaluates the picked light's full BRDF response, divides by the RIS
*     selection pdf (weight_sum / picked_weight) to keep the single-sample
*     estimate unbiased, and multiplies by the shadow ray's visibility. The
*     result is a noisy single-sample RGB radiance estimate rather than a
*     scalar - ShadowDenoiseCS.hlsl accumulates it over time (tracking
*     variance from its luminance, since a full RGB covariance would triple
*     the moment storage for no denoising benefit). This replaces sharing one
*     averaged visibility scalar across every punctual light (like UE Lumen's
*     stochastic shadowing) - dozens of lights can affect one pixel, so tracing
*     one full shadow ray per light per frame does not scale, but averaging
*     their visibility together was wrong: a light that is actually occluded
*     would only partially darken, blended with whichever other light in the
*     cluster happened to be lit.
*
* Output: raw_visibility_uav_index_, r = directional visibility, gba = the
* picked punctual light's RGB radiance estimate, both raw/noisy.
*
* ---------------------------------------------------------------------
*
* [JP]
* 確率的レイトレ影 + ReSTIR DI パス(インライン RayQuery)。1スレッド=1
* 画面ピクセル、1フレームにつき影レイを1本だけ撃つ:
*   - ディレクショナルライト: 常に評価する(主光源1灯だけなので安い)。
*     sun_angular_radius_ で決まるコーン内でレイをジッターしてソフトな
*     半影を作る。出力は従来通りスカラー可視性のまま。
*   - Point/Spot/Rect ライト: このピクセルを照らす候補から重み付き
*     リザーバーサンプリングで毎フレーム【1灯だけ】選ぶ(灯数が増えても
*     コストが増えない) - 重みは各光の実効寄与(強度×距離減衰×スポット
*     フェード、面に対して背面/スポット圏外/矩形の裏側なら0)なので、この
*     ピクセルを実際には照らしていない光が選ばれて影レイを無駄にすることは
*     ない。選ばれた光の表面上/近傍のランダムな点へレイをジッターする
*     (物理サイズがそのまま矩形光のソフトシャドウになる。Point/Spot は
*     固定のワールド空間半径)。ディレクショナル項と違い、こちらは完全な
*     ReSTIR DI 型の推定量: このピクセルの G-Buffer マテリアルを解決し
*     (Shader/Material.hlsli の ResolveGBufferMaterial - Model/Opaque/
*     DeferredLightingPS.hlsl と【同じ】関数を使うので鏡面ハイライトの
*     形が一致する)、選ばれた光の完全な BRDF 応答を評価し、RIS の選択
*     pdf(weight_sum / picked_weight)で割って単一サンプル推定量を
*     アンバイアスに保ち、影レイの可視性を掛ける。結果はスカラーではなく
*     ノイズを含む単一サンプルの RGB 放射輝度推定量になる -
*     ShadowDenoiseCS.hlsl がこれを時間積分する(モーメントは輝度から追跡
*     する - RGB 共分散をフルで持つとモーメントの保存量が3倍になり、
*     デノイズの得にはならないため)。これは、UE Lumen の確率的シャドウの
*     ように可視性スカラー1つを全パンクチュアルライトで共有する方式を
*     置き換える - 1ピクセルに何十灯も影響し得るので毎フレーム全灯に
*     フルの影レイを撃つのはスケールしないが、可視性を平均するのは誤り
*     だった: 実際に遮蔽されている光は部分的にしか暗くならず、クラスタ内の
*     たまたま照射中の別の光と混ざってしまう。
*
* 出力: raw_visibility_uav_index_、r = ディレクショナル可視性、gba = 選ばれた
* パンクチュアルライトの RGB 放射輝度推定量、どちらも生値/ノイズ入り。
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
	RWTexture2D<float4> raw_signal = ResourceDescriptorHeap[structured_indices.shadow_.raw_visibility_uav_index_];

	/// [EN] Sample scene depth. With reverse-Z the far plane is 0.0, so a
	///      depth of 0 means "nothing rendered here" (background) - there is
	///      no surface to shadow, so it is treated as fully lit (1.0).
	/// [JP] シーン深度をサンプルする。reverse-Z では遠平面が 0.0 なので、
	///      深度0は「何も描画されていない」(背景)を意味する。影を落とす
	///      対象の面が無いので、完全照射(1.0)として扱う。
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		raw_signal[pixel] = float4(1.0, 0.0, 0.0, 0.0);
		return;
	}

	/// [EN] Reconstruct this pixel's world position: pixel -> uv -> NDC
	///      (texture uv is flipped vertically, so y is inverted), use depth
	///      as z and invert the view-projection. inverse_view_projection_ is
	///      row_major, so the row vector goes on the left of mul(). The
	///      result is homogeneous, so divide by w to recover the actual 3D
	///      position.
	/// [JP] このピクセルのワールド座標を復元する。pixel -> uv -> NDC
	///      (テクスチャ uv は上下逆なので y を反転)、深度を z として使い、
	///      ビュープロジェクションを逆算する。inverse_view_projection_ は
	///      row_major なので、行ベクトルは mul() の左に置く。変換後は
	///      同次座標なので、w で割って実際の 3D 座標に戻す。
	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
	float4 clip = float4(ndc, depth, 1.0);
	float4 world = mul(clip, scene.inverse_view_projection_);
	float3 world_position = world.xyz / world.w;

	ConstantBuffer<ShadowRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.shadow_.ray_constant_index_];

	/// [EN] The normal is stored oct-encoded in G-Buffer RT1, so decode it.
	///      The ray origin is pushed off the surface along the normal by
	///      normal_bias_ to avoid self-intersection (shadow acne).
	/// [JP] 法線は G-Buffer RT1 に oct エンコードで格納されているので
	///      デコードする。レイ原点は自己交差(シャドウアクネ)を避けるため
	///      normal_bias_ ぶん法線方向へ押し出す。
	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float4 rt1 = normal_texture.Load(int3(pixel, 0));
	float3 normal = OctNormalDecode(rt1.rg);
	float3 origin = world_position + normal * tuning.normal_bias_;

	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[structured_indices.raytracing_.tlas_index_];

	/// [EN] RNG seed that changes every frame. Varying the ray direction each
	///      frame is what lets temporal accumulation converge to a smooth
	///      penumbra instead of a fixed dithered pattern.
	/// [JP] 毎フレーム変わる RNG シード。レイ方向を毎フレーム変えることで、
	///      時間積分後になめらかな半影へ収束する(固定パターンのディザに
	///      ならない)。
	uint rng_state = SeedFromPixel(pixel, tuning.frame_index_);

	/// ---- Directional light (always evaluated - a single dominant light, no probabilistic selection needed) ----
	/// ---- ディレクショナルライト(常に評価。1本で済む主光源なので確率選択しない) ----
	float directional_visibility = 1.0;
	ConstantBuffer<LightConstantData> light = ResourceDescriptorHeap[constant_indices.light_index_];
	float3 directional_base = normalize(-light.directional_direction_);

	if (light.directional_intensity_ > 0.0)
	{
		/// [EN] A surface facing away from the light (N.L<=0) receives zero
		///      direct light, so there is no point tracing a ray. Explicitly
		///      mark it unoccluded-as-in-shadow (visibility 0) to save the
		///      wasted ray and its noise.
		/// [JP] 光に背いた面(N・L<=0)は直接光がゼロなのでレイを飛ばす意味が
		///      ない。明示的に可視性0(影)としてスキップし、無駄なレイと
		///      ノイズを減らす。
		if (dot(normal, directional_base) <= 0.0)
		{
			directional_visibility = 0.0;
		}
		else
		{
			float cos_theta_max = cos(tuning.sun_angular_radius_);
			float3 ray_direction = SampleCone(rng_state, directional_base, cos_theta_max);

			RayDesc ray_desc;
			ray_desc.Origin = origin;
			ray_desc.Direction = ray_direction;
			ray_desc.TMin = 0.001;
			ray_desc.TMax = tuning.ray_t_max_;

			directional_visibility = IsReflectionRayOccluded(tlas, ray_desc, structured_indices.raytracing_.instance_data_index_) ? 0.0 : 1.0;
		}
	}

	/// ---- Point/Spot/Rect (probabilistically pick just one from this pixel's cluster, ReSTIR DI) ----
	/// ---- Point/Spot/Rect(このピクセルのクラスタから確率的に1灯だけ選ぶ、ReSTIR DI) ----
	float3 punctual_radiance = float3(0, 0, 0);

	float4 view_position = mul(float4(world_position, 1.0), scene.view_);
	float linear_depth = view_position.z;

	uint3 cluster_count = ComputeClusterCount(scene.screen_size_);
	uint tile_x = pixel.x / CLUSTER_TILE_SIZE;
	uint tile_y = pixel.y / CLUSTER_TILE_SIZE;
	uint slice = ComputeDepthSlice(linear_depth, scene.near_plane_, scene.far_plane_);
	uint cluster_index = ClusterIndex(uint3(tile_x, tile_y, slice), cluster_count);

	StructuredBuffer<ClusterData> cluster_data = ResourceDescriptorHeap[light.cluster_data_shader_resource_view_index_];
	ClusterData cluster = cluster_data[cluster_index];

	uint point_count = min(cluster.point_count_, CLUSTER_MAX_POINT_LIGHTS);
	uint spot_count = min(cluster.spot_count_, CLUSTER_MAX_SPOT_LIGHTS);
	uint rect_count = min(cluster.rect_count_, CLUSTER_MAX_RECT_LIGHTS);
	uint total_punctual = point_count + spot_count + rect_count;

	if (total_punctual > 0)
	{
		ByteAddressBuffer light_list = ResourceDescriptorHeap[light.cluster_light_list_shader_resource_view_index_];
		uint base = cluster_index * CLUSTER_STRIDE;

		StructuredBuffer<PointLightData> point_lights = ResourceDescriptorHeap[light.point_light_index_];
		StructuredBuffer<SpotLightData> spot_lights = ResourceDescriptorHeap[light.spot_light_index_];
		StructuredBuffer<RectLightData> rect_lights = ResourceDescriptorHeap[light.rect_light_index_];

		/// [EN] Weighted reservoir sampling (a simplified single-pass form of
		///      Algorithm A-Res). Picking uniformly at random would waste the
		///      one shadow ray equally often on lights that don't actually
		///      illuminate this pixel at all - outside a spot's cone, behind
		///      a rect light, out of range, or facing away from the normal
		///      (N.L<=0) - which starves the actually-dominant light of
		///      updates and slows convergence / leaves visible noise. Using
		///      each light's effective contribution (intensity * distance
		///      attenuation * spot fade, zero contribution => zero weight =>
		///      excluded) as the weight makes brighter/more-relevant lights
		///      sampled more often. Same shape as ReSTIR's RIS (Resampled
		///      Importance Sampling) - picked_weight/weight_sum becomes the
		///      selection pdf, divided out below when evaluating the BRDF
		///      response to keep the single-sample estimate unbiased.
		/// [JP] 重み付きリザーバーサンプリング(Algorithm A-Res の単純化版、
		///      1パス)。一様ランダムに選ぶと、Spot の照射コーン外・Rect の
		///      裏面・レンジ外・法線に対して背面(N・L<=0)など「そもそも
		///      このピクセルを照らしていない光」にも同じ確率でレイを浪費
		///      してしまい、実際に支配的な光の更新頻度が下がって収束が
		///      遅れる/ノイズが残る原因になっていた。各光の実効寄与度
		///      (強度×距離減衰×スポットフェード、寄与ゼロなら重み0で除外)を
		///      重みとして使うことで、明るく効いている光ほど高頻度で
		///      サンプルされるようにする。ReSTIR の RIS(Resampled
		///      Importance Sampling)と同じ形 - picked_weight/weight_sum が
		///      選択pdfになり、下でBRDF応答をこれで割って単一サンプル
		///      推定量をアンバイアスに保つ。
		float weight_sum = 0.0;
		float picked_weight = 0.0;
		uint picked = 0xFFFFFFFF;

		for (uint index = 0; index < total_punctual; index++)
		{
			float weight = 0.0;

			if (index < point_count)
			{
				uint light_index = light_list.Load((base + index) * 4);
				PointLightData point_light = point_lights[light_index];

				float3 to_light = point_light.position - world_position;
				float distance_to_light = length(to_light);
				float3 light_direction = to_light / max(distance_to_light, 0.0001);

				if (dot(normal, light_direction) > 0.0)
				{
					weight = point_light.intensity * AttenuateDistance(distance_to_light, point_light.range);
				}
			}
			else if (index < point_count + spot_count)
			{
				uint local_index = index - point_count;
				uint light_index = light_list.Load((base + CLUSTER_MAX_POINT_LIGHTS + local_index) * 4);
				SpotLightData spot_light = spot_lights[light_index];

				float3 to_light = spot_light.position - world_position;
				float distance_to_light = length(to_light);
				float3 light_direction = to_light / max(distance_to_light, 0.0001);

				if (dot(normal, light_direction) > 0.0)
				{
					float cos_angle = dot(-light_direction, spot_light.direction);
					float spot_fade = saturate((cos_angle - spot_light.cos_half_angle) / max(spot_light.softness * (1.0 - spot_light.cos_half_angle), 0.0001));
					weight = spot_light.intensity * AttenuateDistance(distance_to_light, spot_light.range) * spot_fade;
				}
			}
			else
			{
				uint local_index = index - point_count - spot_count;
				uint light_index = light_list.Load((base + CLUSTER_MAX_POINT_LIGHTS + CLUSTER_MAX_SPOT_LIGHTS + local_index) * 4);
				RectLightData rect_light = rect_lights[light_index];

				if (dot(world_position - rect_light.position, rect_light.normal) > 0.0)
				{
					float3 representative_point = ClosestPointOnRect(world_position, rect_light.position, rect_light.right, rect_light.up, rect_light.half_width, rect_light.half_height);
					float3 to_light = representative_point - world_position;
					float distance_to_light = length(to_light);
					float3 light_direction = to_light / max(distance_to_light, 0.0001);

					if (dot(normal, light_direction) > 0.0)
					{
						weight = rect_light.intensity * AttenuateDistance(distance_to_light, rect_light.range);
					}
				}
			}

			if (weight > 0.0)
			{
				weight_sum += weight;

				/// [EN] Rand() is PcgNext's `y / 4294967295.0`, so it CAN
				///      return exactly 1.0. Writing `Rand() < weight /
				///      weight_sum` would reject the very first valid light
				///      the instant its ratio hits exactly 1.0, and if every
				///      later candidate is also rejected, picked stays
				///      0xFFFFFFFF and falls through to the rect branch
				///      below with a huge local_index. Written as a
				///      multiplication instead, weight_sum > 0 guarantees
				///      weight <= weight_sum, so on the first candidate
				///      Rand()*weight_sum <= weight always holds and picked
				///      is always resolved.
				/// [JP] Rand() は PcgNext の `y / 4294967295.0` なので 1.0 を
				///      「含む」。`Rand() < weight / weight_sum` と書くと、
				///      最初の有効光(比が必ず 1.0)で 1.0 を引いた瞬間に
				///      採用されず、以降も外れ続けると picked が
				///      0xFFFFFFFF のまま下の分岐へ落ちて rect の
				///      local_index が巨大値になる。乗算形にすれば
				///      weight_sum > 0 のとき必ず weight <= weight_sum
				///      なので、初回は Rand()*weight_sum <= weight が
				///      成り立ち、picked は必ず確定する。
				if (Rand(rng_state) * weight_sum <= weight)
				{
					picked = index;
					picked_weight = weight;
				}
			}
		}

		/// [EN] Normally unreachable given the guarantee above, but folds
		///      picked back to a safe "nothing selected" state if rounding
		///      ever left it unresolved, so an out-of-range index is never
		///      constructed below.
		/// [JP] 上の保証があるので通常ここは通らないが、丸め次第で picked
		///      が未確定のまま来た場合に範囲外インデックスを作らないよう
		///      畳んでおく。
		if (picked >= total_punctual)
		{
			weight_sum = 0.0;
		}

		if (weight_sum > 0.0)
		{
			float3 sample_point;
			float3 light_color;
			float3 base_direction;
			float light_radius = tuning.punctual_light_radius_;

			if (picked < point_count)
			{
				uint light_index = light_list.Load((base + picked) * 4);
				PointLightData point_light = point_lights[light_index];
				sample_point = point_light.position;
				light_color = point_light.color.rgb;
			}
			else if (picked < point_count + spot_count)
			{
				uint local_index = picked - point_count;
				uint light_index = light_list.Load((base + CLUSTER_MAX_POINT_LIGHTS + local_index) * 4);
				SpotLightData spot_light = spot_lights[light_index];
				sample_point = spot_light.position;
				light_color = spot_light.color.rgb;
			}
			else
			{
				uint local_index = picked - point_count - spot_count;
				uint light_index = light_list.Load((base + CLUSTER_MAX_POINT_LIGHTS + CLUSTER_MAX_SPOT_LIGHTS + local_index) * 4);
				RectLightData rect_light = rect_lights[light_index];

				/// [EN] An area light only needs a random point on its own
				///      surface sampled - the physical size then naturally
				///      produces a penumbra scaled to the light's size (no
				///      cone jitter needed).
				/// [JP] 面光源はその面上のランダムな点をサンプルするだけで、
				///      物理サイズに応じた半影が自然に出る(コーンジッター
				///      不要)。
				float2 rect_uv = Rand2(rng_state) * 2.0 - 1.0;
				sample_point = rect_light.position + rect_light.right * (rect_uv.x * rect_light.half_width) + rect_light.up * (rect_uv.y * rect_light.half_height);
				light_color = rect_light.color.rgb;
				light_radius = 0.0;
			}

			float3 to_light = sample_point - world_position;
			float distance_to_light = length(to_light);
			base_direction = to_light / max(distance_to_light, 0.0001);

			/// [EN] Point/Spot derive the cone's half-angle from a
			///      world-space radius (sin(half_angle) = radius /
			///      distance). Area lights are already softened by their
			///      sample point, so no extra jitter is applied
			///      (cos_theta_max = 1).
			/// [JP] Point/Spot はワールド空間半径から円錐の半頂角を導く
			///      (sin(half_angle) = radius / distance)。面光源は既に
			///      サンプル点でソフトになっているので追加ジッターは
			///      無し(cos_theta_max = 1)。
			float sin_theta_max = saturate(light_radius / max(distance_to_light, 0.001));
			float cos_theta_max = sqrt(max(0.0, 1.0 - sin_theta_max * sin_theta_max));
			float3 ray_direction = (light_radius > 0.0) ? SampleCone(rng_state, base_direction, cos_theta_max) : base_direction;

			RayDesc ray_desc;
			ray_desc.Origin = origin;
			ray_desc.Direction = ray_direction;
			ray_desc.TMin = 0.001;
			ray_desc.TMax = max(distance_to_light - 0.01, 0.001);

			float ray_visibility = IsReflectionRayOccluded(tlas, ray_desc, structured_indices.raytracing_.instance_data_index_) ? 0.0 : 1.0;
			float shadow_factor = saturate(lerp(1.0, ray_visibility, max(tuning.shadow_strength_, 0.0)));

			/// [EN] Resolve this pixel's G-Buffer material. Using the exact
			///      same ResolveGBufferMaterial as DeferredLightingPS.hlsl
			///      makes the specular highlight shape here match the one
			///      seen from the primary (G-Buffer) view.
			/// [JP] この面の G-Buffer マテリアルを解決する。
			///      DeferredLightingPS.hlsl と全く同じ ResolveGBufferMaterial
			///      を使うことで、鏡面ハイライトの形が主要視点(G-Buffer)側と
			///      一致する。
			Texture2D<float4> gbuffer0 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_0_];
			float4 rt0 = gbuffer0.Load(int3(pixel, 0));
			float3 base_color = rt0.rgb;
			float metallic = rt0.a;
			float roughness = rt1.b;

			Texture2D<uint4> gbuffer4 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_4_];
			uint4 visibility_id = gbuffer4.Load(int3(pixel, 0));
			uint material_instance_index, material_meshlet_index, material_triangle_index;
			UnpackVisibilityID(visibility_id, material_instance_index, material_meshlet_index, material_triangle_index);
			StructuredBuffer<ModelInstance> material_instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
			ModelInstance material_instance = material_instances[material_instance_index];

			float3 view = normalize(scene.camera_position_.xyz - world_position);
			float2 material_texcoord = UnpackVisibilityTexcoord(visibility_id);
			GBufferMaterial gbuffer_material = ResolveGBufferMaterial(base_color, metallic, roughness, normal, view, material_instance, material_texcoord, rt1.a);

			float3 brdf_response = EvalDirectLightDispatch(material_instance.shading_model_, normal, view, base_direction, gbuffer_material.diffuse_color_, gbuffer_material.f0_, gbuffer_material.roughness_, gbuffer_material.clearcoat_factor_, gbuffer_material.clearcoat_roughness_, gbuffer_material.clearcoat_normal_, gbuffer_material.sheen_color_, gbuffer_material.sheen_roughness_, gbuffer_material.anisotropy_tangent_, gbuffer_material.anisotropy_bitangent_, gbuffer_material.anisotropy_strength_);

			/// [EN] Divide by the RIS selection pdf (picked_weight/weight_sum)
			///      to keep the single-sample estimate unbiased (result =
			///      f(picked) / pdf(picked) = f(picked) * weight_sum /
			///      picked_weight). f(picked) is brdf_response*light_color
			///      (this light's actual contribution to this pixel) -
			///      picked_weight (intensity * distance attenuation * spot
			///      fade, no color) is purely a selection weight and is not
			///      multiplied into f(picked).
			/// [JP] RIS の選択pdf(picked_weight/weight_sum)で割って単一
			///      サンプル推定量をアンバイアスに保つ(result = f(picked) /
			///      pdf(picked) = f(picked) * weight_sum / picked_weight)。
			///      f(picked)は brdf_response*light_color(このピクセルへの
			///      この光の実際の寄与) - picked_weight(強度×距離減衰×
			///      スポットフェード、色は含まない)はあくまで選択用の重みで、
			///      f(picked)には掛けない。
			float inverse_pdf = weight_sum / max(picked_weight, 0.0001);
			punctual_radiance = brdf_response * light_color * inverse_pdf * shadow_factor;
		}
	}

	raw_signal[pixel] = float4(directional_visibility, punctual_radiance);
}

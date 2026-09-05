#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../Reflection/Reflection.hlsli"
#include "SubsurfaceScattering.hlsli"

/**
* [EN]
* Reference:
* - https://colinbarrebrisebois.com/2011/03/07/gdc-2011-approximating-translucency-for-a-fast-cheap-and-convincing-subsurface-scattering-look/
*   (Barre-Brisebois & Bouchard, "Approximating Translucency for a Fast,
*   Cheap and Convincing Subsurface Scattering Look", GDC 2011 - the
*   thickness-based transmittance approximation this pass measures via
*   raytracing instead of the paper's screen-space depth trick.)
* - https://www.iryoku.com/translucency/
*   (Jimenez et al., "Real-Time Realistic Skin Translucency", IEEE CG&A
*   2010 - the earlier thickness -> transmittance formulation
*   Barre-Brisebois's technique builds on; transmittance = exp(-thickness /
*   scatter_distance_) below is the same family of exponential falloff.)
*
* Ray-traced subsurface scattering / translucency pass (inline RayQuery,
* thickness-measurement based). One thread per screen pixel, ONE deterministic
* ray per pixel per frame: pushes the ray origin slightly INTO the surface
* (along -normal) and traces toward the directional light to find the first
* exit face - that hit distance is the object's local thickness. Writes
* transmittance = exp(-thickness / scatter_distance_) (0 = opaque, 1 = fully
* translucent). No RNG and no denoiser needed: the ray is the same every
* frame, so the output is stable (unlike the stochastic shadow/AO passes).
* DeferredLightingPS.hlsl turns this into a back-lit translucency term.
*
* ---------------------------------------------------------------------
*
* [JP]
* レイトレ表面下散乱/透光パス(インライン RayQuery、厚み計測ベース)。
* 1スレッド=1ピクセル、1フレームに決定論的なレイ1本: レイ原点を表面の少し
* 内側(-法線方向)へ押し込み、ディレクショナルライトへ向かって撃って最初の
* 出口面までの距離=そこの局所的な厚みを測る。
* transmittance = exp(-厚み / scatter_distance_) (0=不透明、1=完全透光)を
* 書き込む。乱数もデノイザも不要: レイは毎フレーム同じなので出力は安定
* (確率的な影/AOパスとは違う)。DeferredLightingPS.hlsl がこれを
* 裏面ライティングの透光項に変換する。
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
	RWTexture2D<float> transmittance_output = ResourceDescriptorHeap[structured_indices.subsurface_scattering_.transmittance_uav_index_];

	/// [EN] Background (reverse-Z far plane = 0) has no translucency = 0.0.
	/// [JP] 背景(reverse-Z 遠平面=0)は透光なし=0.0。
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		transmittance_output[pixel] = 0.0;
		return;
	}

	/// [EN] No point measuring without a directional light.
	/// [JP] ディレクショナルライトが無ければ測る意味がない。
	ConstantBuffer<LightConstantData> light = ResourceDescriptorHeap[constant_indices.light_index_];
	if (light.directional_intensity_ <= 0.0)
	{
		transmittance_output[pixel] = 0.0;
		return;
	}

	/// [EN] Reconstruct world position and normal (same procedure as
	///      ShadowRT.hlsl, mul takes the row vector on the left).
	/// [JP] ワールド座標と法線を復元する(ShadowRT.hlsl と同じ手順、mul は
	///      行ベクトル左)。
	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
	float4 clip = float4(ndc, depth, 1.0);
	float4 world = mul(clip, scene.inverse_view_projection_);
	float3 world_position = world.xyz / world.w;

	ConstantBuffer<SubsurfaceScatteringRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.subsurface_scattering_.ray_constant_index_];

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float3 normal = OctNormalDecode(normal_texture.Load(int3(pixel, 0)).rg);

	float3 light_direction = normalize(-light.directional_direction_);

	/// [EN] A surface lit from the front (N.L>0) is ordinary lighting's
	///      territory, not translucency. Translucency only means something
	///      for a surface facing away from the light, so everything else is
	///      skipped.
	/// [JP] 光が表から当たっている面(N・L>0)は透光ではなく通常のライティング
	///      の領分。透光が意味を持つのは光に背いた面だけなので、それ以外は
	///      スキップ。
	if (dot(normal, light_direction) >= 0.0)
	{
		transmittance_output[pixel] = 0.0;
		return;
	}

	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[structured_indices.raytracing_.tlas_index_];

	/// [EN] Fire from slightly inside the surface toward the light and
	///      measure the distance to the first exit face = thickness. No
	///      ACCEPT_FIRST_HIT is set, since the closest hit is needed (an
	///      any-hit result could commit any intersection first, potentially
	///      overestimating thickness).
	///
	///      At a near-grazing angle to light_direction (N.L only barely
	///      negative), the mismatch between the interpolated shading normal
	///      and the mesh's actual triangle face can, with the normal offset
	///      alone, immediately stab the ray into its own or a neighboring
	///      triangle (the same problem AmbientOcclusionRT.hlsl also pushes
	///      the origin along the ray direction to avoid). If this happens
	///      here, the measured thickness collapses to nearly 0, giving
	///      transmittance = exp(0) approx 1.0 (nearly fully translucent) by
	///      mistake, and SSS color leaks strongly onto a surface that
	///      should not be translucent at all (the cause of a bug where
	///      entire surfaces facing away from the light glowed faintly). This
	///      is avoided by pushing the origin along the ray direction as
	///      well as the normal bias.
	/// [JP] 表面の少し内側から光源方向へ撃ち、最初の出口面までの距離=厚みを
	///      測る。最近接ヒットが欲しいので ACCEPT_FIRST_HIT は付けない
	///      (any-hit だと最初にコミットされた任意の交点になり、厚みが
	///      過大評価されうる)。
	///
	///      light_direction とほぼすれすれの角度(N・L がわずかにマイナス
	///      なだけのグレージング角)では、補間されたシェーディング法線と
	///      実際の三角形面のズレにより、法線方向のオフセットだけでは自分
	///      自身や隣接三角形へレイがすぐ刺さってしまう
	///      (AmbientOcclusionRT.hlsl が同じ理由でレイ方向にも原点を押し
	///      出しているのと同じ問題)。厚み計測でこれが起きると thickness が
	///      ほぼ 0 になり、transmittance = exp(0) ≈ 1.0(ほぼ完全透光)が
	///      誤って出て、本来透けないはずの不透明面に SSS の色が強く漏れる
	///      (光に背いた面全体が薄く光って見えるバグの原因)。法線バイアスに
	///      加え、レイ方向にも原点を押し出してこれを避ける。
	RayDesc ray_desc;
	ray_desc.Origin = world_position - normal * tuning.thickness_bias_ + light_direction * tuning.thickness_bias_;
	ray_desc.Direction = light_direction;
	ray_desc.TMin = 0.001;
	ray_desc.TMax = tuning.ray_t_max_;

	/// [EN] The translucency thickness needs the distance to the closest
	///      hit, so the candidate loop is run directly here instead of the
	///      occlusion-only IsReflectionRayOccluded.
	/// [JP] 透光の厚みは最近接ヒットまでの距離が要るため、遮蔽判定用の
	///      IsReflectionRayOccluded ではなく候補ループを直接回す。
	RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
	query.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, ray_desc);

	while (query.Proceed())
	{
		if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
		{
			if (!IsReflectionMaterialPassthrough(structured_indices.raytracing_.instance_data_index_, query.CandidateInstanceID(), query.CandidatePrimitiveIndex(), query.CandidateTriangleBarycentrics()))
			{
				query.CommitNonOpaqueTriangleHit();
			}
		}
	}

	if (query.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
	{
		/// [EN] No exit face found = thicker than ray_t_max_ (or a gap in
		///      the geometry). No translucency.
		/// [JP] 出口が見つからない = ray_t_max_ より厚い(または隙間)。
		///      透光なし。
		transmittance_output[pixel] = 0.0;
		return;
	}

	float thickness = query.CommittedRayT();
	float transmittance = exp(-thickness / max(tuning.scatter_distance_, 0.0001));

	transmittance_output[pixel] = transmittance;
}

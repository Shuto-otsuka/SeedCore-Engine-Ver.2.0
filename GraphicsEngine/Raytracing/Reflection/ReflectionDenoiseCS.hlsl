#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Denoiser.hlsli"

static const float REFLECTION_TEMPORAL_BLEND_ALPHA = 0.1;

// [JP] 分散クリッピングの許容幅。GI と同じ TAA 典型値。
static const float REFLECTION_HISTORY_CLIP_GAMMA = 1.0;

static const float REFLECTION_DEPTH_SHARPNESS = 48.0;
static const float REFLECTION_NORMAL_POWER = 8.0;

// [JP] 近傍のラフネス差に対する重み減衰の鋭さ。GGXサンプル方向は面法線
//      まわりに roughness に応じて広がるため、深度/法線が近くてもラフネスが
//      違う近傍(=違うマテリアル)を混ぜるとハイライトの輪郭が滲む。大きい
//      ほど「ラフネスが近い近傍だけ」に絞り込む。
static const float REFLECTION_ROUGHNESS_SHARPNESS = 24.0;

/**
* [EN]
* Spatio-temporal denoiser for the raw 1spp GGX-importance-sampled
* reflection radiance (ReflectionRT.hlsl's output). Same overall structure as
* GlobalIlluminationDenoiseCS.hlsl (5x5 depth/normal-weighted bilateral
* average + 3x3 variance-clipped temporal reprojection via Denoiser.hlsli),
* with one addition: the spatial weight also fades out neighbors whose
* roughness differs from the center pixel's, since the GGX half-vector lobe
* width is roughness-dependent - reusing a smooth neighbor's tight highlight
* for a rough pixel (or vice versa) blurs/sharpens the wrong amount. At
* roughness 0 ReflectionRayGeneration's importance sampling degenerates to
* the exact mirror direction every frame (no noise at all), so this filter's
* only real work is on rough surfaces, where a same-roughness-only blur is
* visually appropriate anyway.
*
* Known limitation (not solved here): this is still a same-depth/normal/
* roughness bilateral reuse, not a full reflection-aware resampler (e.g.
* ReSTIR) - two adjacent pixels on the same smooth-ish surface that happen to
* reflect very different distant geometry can still bleed into each other a
* little. Acceptable for v1; revisit if it becomes visible in practice.
*
* [JP]
* 生の 1spp GGX重点サンプリング反射放射輝度(ReflectionRT.hlsl の出力)の
* 空間+時間デノイザ。GlobalIlluminationDenoiseCS.hlsl と同じ全体構成(5x5の
* 深度/法線重み付きバイラテラル平均 + Denoiser.hlsli 経由の3x3分散クリッピング
* 時間的リプロジェクション)に、1点だけ追加: 空間重みが、中心ピクセルと
* ラフネスの違う近傍も減衰させる。GGXハーフベクトルのローブ幅は roughness に
* 依存するため、滑らかな近傍の鋭いハイライトを粗いピクセルへ流用する(逆も
* 同様)と、ぼけ/シャープさの量を誤る。roughness 0 では
* ReflectionRayGeneration の重点サンプリングが毎フレーム厳密なミラー方向へ
* 縮退する(ノイズが全く出ない)ので、このフィルタが実際に効くのは粗い面
* だけであり、そこでは「ラフネスが近い近傍だけ」でぼかすのは見た目的にも
* 妥当。
*
* 既知の制限(ここでは解決していない): あくまで同深度/法線/ラフネスでの
* バイラテラル再利用であり、本格的な反射専用リサンプラ(ReSTIR等)ではない
* - 同じような滑らかさの面でも隣接ピクセルが全く違う遠景を映している場合、
* わずかに滲むことがある。v1としては許容範囲、実際に目立つようなら見直す。
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
	Texture2D<float4> raw_reflection = ResourceDescriptorHeap[structured_indices.reflection_.output_srv_index_];
	RWTexture2D<float4> accumulated_reflection = ResourceDescriptorHeap[constant_indices.reflection_.accumulated_uav_index_];

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		accumulated_reflection[pixel] = float4(0, 0, 0, 0);
		return;
	}

	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;

	// velocity は (current_ndc - previous_ndc) * 0.5 で書き込まれている。
	// NDC->UV 変換で y は反転するため、UV 空間の移動量は (velocity.x, -velocity.y)。
	Texture2D<float2> velocity_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_3_];
	float2 velocity = velocity_texture.Load(int3(pixel, 0));
	float2 delta_uv = float2(velocity.x, -velocity.y);
	float2 previous_uv = uv - delta_uv;

	// gbuffer1: rg = oct法線、b = ラフネス(DeferredCompositePS.hlsl と同じ
	// 詰め方)。
	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float4 center_gbuffer1 = normal_texture.Load(int3(pixel, 0));
	float3 center_normal = OctNormalDecode(center_gbuffer1.rg);
	float center_roughness = center_gbuffer1.b;

	int2 screen_max = int2(scene.screen_size_) - 1;

	float2 depth_gradient = DenoiserDepthGradient(depth_texture, int2(pixel), screen_max);

	float weight_sum = 0.0;
	float3 filtered_raw = float3(0, 0, 0);

	DenoiserMoments moments = DenoiserMomentsInit();

	[unroll]
	for (int dy = -2; dy <= 2; ++dy)
	{
		[unroll]
		for (int dx = -2; dx <= 2; ++dx)
		{
			int2 neighbor = clamp(int2(pixel) + int2(dx, dy), int2(0, 0), screen_max);
			float4 neighbor_raw = raw_reflection.Load(int3(neighbor, 0));
			float neighbor_depth = depth_texture.Load(int3(neighbor, 0));
			float4 neighbor_gbuffer1 = normal_texture.Load(int3(neighbor, 0));
			float3 neighbor_normal = OctNormalDecode(neighbor_gbuffer1.rg);
			float neighbor_roughness = neighbor_gbuffer1.b;

			float spatial_weight = DenoiserSpatialWeight(depth, depth_gradient, int2(dx, dy), neighbor_depth, center_normal, neighbor_normal, REFLECTION_DEPTH_SHARPNESS, REFLECTION_NORMAL_POWER);
			float roughness_weight = exp(-abs(center_roughness - neighbor_roughness) * REFLECTION_ROUGHNESS_SHARPNESS);

			// 背景/無効な近傍(ReflectionRayGeneration が a=0 で書いた画素)は
			// 完全に除外する。
			float weight = spatial_weight * roughness_weight * neighbor_raw.a;

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
		filtered_raw = raw_reflection.Load(int3(pixel, 0)).rgb;
	}

	float3 clip_min = DenoiserVarianceClipMin(moments, filtered_raw, REFLECTION_HISTORY_CLIP_GAMMA);
	float3 clip_max = DenoiserVarianceClipMax(moments, filtered_raw, REFLECTION_HISTORY_CLIP_GAMMA);

	Texture2D<float4> history_reflection = ResourceDescriptorHeap[constant_indices.reflection_.history_srv_index_];
	float3 result = DenoiserTemporalBlend(history_reflection, previous_uv, clip_min, clip_max, filtered_raw, REFLECTION_TEMPORAL_BLEND_ALPHA);

	accumulated_reflection[pixel] = float4(result, 1.0);
}

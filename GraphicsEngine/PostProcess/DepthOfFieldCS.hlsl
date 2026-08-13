#include "../Shader/Constants.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [EN]
* Reference:
* - https://blog.voxagon.se/2018/05/04/bokeh-depth-of-field-in-single-pass.html
* - https://www.4rknova.com/blog/2017/01/01/vogel
*
* Depth-of-field gather blur. Runs at native resolution, replacing the HDR
* scene color with a circle-of-confusion (CoC) weighted blur: pixels within
* focus_range_ of focus_distance_ (view-space distance from the camera)
* stay sharp, pixels further away blur up to max_blur_radius_. Samples are
* placed on a Vogel disk (golden-angle spiral) - the standard cheap
* real-time gather-DoF sampling pattern, chosen because it distributes
* DOF_SAMPLE_COUNT samples evenly across a disk with no banding, giving a
* naturally round out-of-focus blur without a separable two-pass blur. This
* is a single-pixel-CoC approximation (no neighbor-CoC search), the same
* approximation level as every other lightweight effect in this
* PostProcess/ folder - a sharp foreground object can bleed softly into a
* blurred background and vice versa.
*
* ---------------------------------------------------------------------
*
* [JP]
* 被写界深度のギャザーブラー。ネイティブ解像度で走り、HDRシーン色を
* 錯乱円(CoC)加重ブラーへ置き換える: focus_distance_(カメラからの
* ビュー空間距離)から focus_range_ 以内のピクセルはシャープなまま、それより
* 遠いピクセルは max_blur_radius_ まで滲む。サンプルは Vogel ディスク
* (黄金角スパイラル)上に配置する — DOF_SAMPLE_COUNT 個のサンプルを
* バンディング無くディスク全体へ均等分布できる、安価なリアルタイム
* ギャザー型DoFの定番パターンで、分離2パスブラーを使わずとも自然な丸い
* ボケが得られる。このピクセル自身のCoCだけを見る近似(近傍CoC探索は
* 行わない)で、この PostProcess/ フォルダの他の軽量エフェクトと同じ近似
* レベル — シャープな手前の物体がぼけた背景ににじむ、あるいはその逆が
* 起こりうる。
*/

#define DOF_SAMPLE_COUNT 16
static const float GOLDEN_ANGLE = 2.39996323;

float3 DepthOfFieldReconstructWorldPosition(float2 uv, float depth, float4x4 inverse_view_projection)
{
	float4 ndc = float4(uv * 2.0 - 1.0, depth, 1.0);
	ndc.y = -ndc.y;
	float4 world_position = mul(ndc, inverse_view_projection);
	return world_position.xyz / world_position.w;
}

float DepthOfFieldLinearViewDepth(float2 uv, Texture2D<float> depth_texture, SceneConstantBuffer scene)
{
	float depth = depth_texture.SampleLevel(sampler_point_clamp, uv, 0);
	float3 world_position = DepthOfFieldReconstructWorldPosition(uv, depth, scene.inverse_view_projection_);
	return mul(float4(world_position, 1.0), scene.view_).z;
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> output = ResourceDescriptorHeap[constant_indices.post_process_.depth_of_field_.unordered_access_view_index_];

	uint width, height;
	output.GetDimensions(width, height);
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.post_process_.source_color_index_];
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);

	float focus_distance = constant_indices.post_process_.depth_of_field_.focus_distance_;
	float focus_range = constant_indices.post_process_.depth_of_field_.focus_range_;
	float max_blur_radius = constant_indices.post_process_.depth_of_field_.max_blur_radius_;

	float center_view_depth = DepthOfFieldLinearViewDepth(uv, depth_texture, scene);
	float center_coc = saturate(abs(center_view_depth - focus_distance) / max(focus_range, 0.0001));
	float blur_radius = center_coc * max_blur_radius;

	float3 accum = source.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
	float weight_sum = 1.0;

	if (blur_radius > 0.0001)
	{
		for (uint i = 0; i < DOF_SAMPLE_COUNT; i++)
		{
			float radius = sqrt((float(i) + 0.5) / float(DOF_SAMPLE_COUNT));
			float angle = float(i) * GOLDEN_ANGLE;
			float2 sample_uv = uv + float2(cos(angle), sin(angle)) * radius * blur_radius;

			accum += source.SampleLevel(sampler_linear_clamp, sample_uv, 0).rgb;
			weight_sum += 1.0;
		}
	}

	output[dtid.xy] = float4(accum / weight_sum, 1.0);
}

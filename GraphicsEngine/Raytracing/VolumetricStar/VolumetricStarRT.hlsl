#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Shader/Noise.hlsli"
#include "VolumetricStar.hlsli"

/**
* [EN]
* Volumetric star pass (screen-space, compute): draws the procedural star
* field, the moon disc and any active shooting star streaks into an RGBA16F
* texture, composited by DeferredLightingPS.hlsl over the procedural sky the
* same way VolumetricCloudScapesRT.hlsl's cloud texture is. Sky pixels only,
* no TLAS. Reads the sun/moon direction and night factor from LightConstantData
* (populated by CelestialSystem via LightSystem::Gather when DaySystem drives
* the frame).
*
* [JP]
* ボリューメトリック・スターパス(スクリーン空間、compute): プロシージャル
* 星空・月ディスク・アクティブな流れ星の筋を RGBA16F テクスチャへ描き、
* DeferredLightingPS.hlsl が VolumetricCloudScapesRT.hlsl の雲テクスチャと
* 同じ手順でプロシージャル空の上に合成する。空ピクセル限定、TLAS 不使用。
* 太陽/月の方向と夜の強さは LightConstantData から読む(DaySystem がこの
* フレームを駆動している時に CelestialSystem が LightSystem::Gather 経由で
* 書き込む)。
*/

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	RWTexture2D<float4> output = ResourceDescriptorHeap[structured_indices.star_.output_uav_index_];

	uint star_width;
	uint star_height;
	output.GetDimensions(star_width, star_height);

	if (dtid.x >= star_width || dtid.y >= star_height)
	{
		return;
	}

	uint2 pixel = dtid.xy;

	// シーンジオメトリがあるピクセルはスキップ(空ピクセル限定)。
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	if (depth_texture.Load(int3(pixel, 0)) != 0.0)
	{
		output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	ConstantBuffer<VolumetricStarRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.star_.ray_constant_index_];

	if (tuning.enabled_ == 0)
	{
		output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	float2 uv = (float2(pixel) + 0.5) / float2(star_width, star_height);
	float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
	float4 far_clip = float4(ndc, 0.0, 1.0);
	float4 far_world = mul(far_clip, scene.inverse_view_projection_);
	float3 view_direction = normalize(far_world.xyz / far_world.w - scene.camera_position_.xyz);

	ConstantBuffer<LightConstantData> light = ResourceDescriptorHeap[constant_indices.light_index_];
	float night_factor = light.night_factor_;

	float3 color = StarFieldColor(view_direction, night_factor, scene.total_time_, tuning);

	if (light.moon_intensity_ > 0.0 || night_factor > 0.0)
	{
		float3 moon_direction = normalize(-light.moon_direction_);
		color += MoonDiscColor(view_direction, moon_direction, light.moon_angular_radius_, light.moon_color_.rgb * max(light.moon_intensity_, 0.05), light.moon_phase_);
	}

	color += ShootingStarColor(view_direction, tuning);

	float alpha = saturate(dot(color, float3(0.333, 0.333, 0.333)));

	output[pixel] = float4(color, alpha);
}

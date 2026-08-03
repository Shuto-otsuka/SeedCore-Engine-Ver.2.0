#include "SkyMath.hlsli"
#include "SkyGenerate.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Sampler.hlsli"
#include "../Shader/Light.hlsli"
#include "../Raytracing/VolumetricCloudScapes/VolumetricCloudScapes.hlsli"

/**
* [EN]
* Renders the procedural sky (gradient + sun, see
* VolumetricCloudScapes.hlsli::ProceduralSkyColor) onto the six faces of the
* environment cube so the existing irradiance/prefilter convolution can turn
* it into IBL - this is how the procedural sky lights the scene when no
* skymap is bound. The environment cube's SRV index is deliberately NOT
* registered in this mode, so the background stays the analytic sky (crisp
* sun disc) while only the convolved IBL cubes are consumed.
* Root bindings: sky_generate (b0 space1, root 2) carries dest/face size and
* - reusing the otherwise-unused source_index_ field - the bindless index of
* the LightConstantData CBV (for the sun direction). structured_indices
* (b1 space1, root 3) resolves the cloud/sky tuning constant buffer.
*
* [JP]
* プロシージャル空(グラデーション+太陽、
* VolumetricCloudScapes.hlsli::ProceduralSkyColor 参照)を environment
* キューブの6面へ描き、既存の irradiance/prefilter 畳み込みで IBL 化する —
* スカイマップ未バインド時にプロシージャル空がシーンを照らす仕組み。
* このモードでは environment キューブの SRV インデックスを意図的に登録しない
* ため、背景は解析的な空(シャープな太陽)のまま、畳み込み済み IBL キューブ
* だけが消費される。
* ルートバインディング: sky_generate(b0 space1、ルート2)が出力先/面サイズと
* — このパスでは未使用の source_index_ を転用して — LightConstantData CBV の
* bindless インデックス(太陽方向用)を運ぶ。structured_indices(b1 space1、
* ルート3)から雲/空チューニング定数バッファを引く。
*/
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint size = sky_generate.face_size_;
	if (id.x >= size || id.y >= size)
	{
		return;
	}

	uint face = id.z + sky_generate.face_offset_;
	float2 uv = (float2(id.xy) + 0.5) / float(size) * 2.0 - 1.0;
	float3 direction = normalize(CubeFaceDirection(face, uv));

	ConstantBuffer<VolumetricCloudScapesRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.cloud_.ray_constant_index_];
	ConstantBuffer<LightConstantData> light = ResourceDescriptorHeap[sky_generate.source_index_];

	float3 sun_direction = normalize(-light.directional_direction_);
	float3 sun_radiance = light.directional_color_.rgb * light.directional_intensity_;

	float3 color = ProceduralSkyColor(direction, sun_direction, sun_radiance, tuning);

	// [JP] 雲もキューブへ焼き込む(縮小版レイマーチ: 固定24ステップ+
	//      ライト3ステップ)。これで反射(prefilter経由)にも IBL にも雲が乗り、
	//      雨雲にすると地上の環境光も暗くなる。キャプチャ視点は地上原点
	//      (0,0,0)。時刻(風スクロール)は sky_generate.roughness_ に転用して
	//      渡される(このパスでは prefilter 用の roughness は未使用)。
	//      IBL は低周波なので適応ステップまでは持ち込まないが、積分式・シェル
	//      形状・多重散乱はスクリーン側と揃える — でないとキューブ由来の環境光
	//      だけが別の明るさになる。
	if (direction.y > 0.001)
	{
		const uint capture_step_count = 24;
		const uint capture_light_step_count = 3;
		float capture_time = sky_generate.roughness_;

		Texture3D<float> shape_noise = ResourceDescriptorHeap[structured_indices.cloud_.shape_noise_srv_index_];
		Texture3D<float> detail_noise = ResourceDescriptorHeap[structured_indices.cloud_.detail_noise_srv_index_];

		float3 ray_origin = float3(0.0, 0.0, 0.0);
		float layer_thickness = max(tuning.cloud_top_ - tuning.cloud_bottom_, 1.0);
		float curvature = CloudCurvature(direction, tuning.planet_radius_);

		float t_enter;
		float t_exit;
		if (CloudLayerInterval(ray_origin, direction, tuning, t_enter, t_exit))
		{
			// [JP] 固定ステップなので、区間が地平線側で数万単位まで伸びると
			//      1ステップがノイズの特徴を飛び越える。焼き込みでは区間を
			//      切ってステップ長を常識的な範囲に収める。
			t_exit = min(t_exit, t_enter + layer_thickness * 8.0);

			float3 cloud_albedo = lerp(tuning.cloud_albedo_, tuning.cloud_albedo_ * 0.35, saturate(tuning.rain_));

			float step_length = (t_exit - t_enter) / float(capture_step_count);
			float cos_theta = dot(direction, sun_direction);

			// [JP] 位相はこの方向の間ずっと定数なのでループ外で一度だけ作る。
			CloudMultiScatter multi_scatter = CloudMultiScatterSetup(cos_theta, tuning);

			float3 scattering = float3(0, 0, 0);
			float transmittance = 1.0;
			float t = t_enter + step_length * 0.5;

			for (uint step = 0; step < capture_step_count; step++)
			{
				float altitude = CloudAltitudeAt(ray_origin.y, direction.y, curvature, t);
				float3 sample_position = ray_origin + direction * t;
				float density = CloudDensity(sample_position, altitude, tuning, capture_time, shape_noise, detail_noise, sampler_linear_wrap, 1.0);

				if (density > 0.0)
				{
					float light_optical_depth = CloudLightOpticalDepth(sample_position, altitude, sun_direction,
						tuning, capture_time, shape_noise, detail_noise, sampler_linear_wrap,
						capture_light_step_count, 0.5);

					float sample_extinction = density * step_length;

					float powder = CloudPowder(light_optical_depth, cos_theta, tuning.powder_strength_);
					float sun_light = CloudSunLight(light_optical_depth, powder, multi_scatter);

					float height01 = saturate((altitude - tuning.cloud_bottom_) / layer_thickness);
					float ambient_occlusion = lerp(0.3, 1.0, height01);
					float3 sky_ambient = lerp(tuning.sky_horizon_color_, tuning.sky_zenith_color_, height01)
						* tuning.sky_brightness_ * tuning.ambient_strength_ * ambient_occlusion;

					float3 in_scattering = cloud_albedo * (sun_radiance * sun_light + sky_ambient);

					float sample_transmittance = exp(-sample_extinction);
					scattering += in_scattering * (1.0 - sample_transmittance) * transmittance;
					transmittance *= sample_transmittance;

					if (transmittance < 0.01)
					{
						break;
					}
				}

				t += step_length;
			}

			color = color * transmittance + scattering;
		}
	}

	RWTexture2DArray<float4> destination = ResourceDescriptorHeap[sky_generate.dest_index_];
	destination[uint3(id.xy, face)] = float4(color, 1.0);
}

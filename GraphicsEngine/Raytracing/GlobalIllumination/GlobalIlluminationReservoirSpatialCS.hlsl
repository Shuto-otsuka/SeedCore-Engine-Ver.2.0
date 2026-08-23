#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Noise.hlsli"
#include "../../Shader/Denoiser.hlsli"
#include "GlobalIllumination.hlsli"

/**
* [EN]
* ReSTIR spatial reuse for GI: runs after GlobalIlluminationRayGeneration has
* written this frame's temporally-combined reservoir for every pixel (and the
* barrier that makes it SRV-readable) — reads the current pixel's own
* reservoir plus a few random neighbors, all from THIS frame's write slot
* (constant_indices.global_illumination_.reservoir_write_srv_index_), and
* streams them together the same way
* GlobalIlluminationRayGeneration folds in temporal history. Reading the
* neighbors from this frame's own data (instead of last frame's, as an
* in-raygen version of this pass would have to) is what makes the reuse
* correct: the depth/normal validity check and the borrowed sample are
* guaranteed to describe the same instant, so a newly-disoccluded pixel
* borrows a neighbor's already-good sample instead of slowly drifting toward
* whatever unrelated geometry happened to occupy that neighbor's pixel one
* frame ago.
*
* Writes the resolved radiance into the same raw texture
* GlobalIlluminationRayGeneration used to write directly
* (structured_indices.global_illumination_.output_uav_index_) — everything
* downstream (GlobalIlluminationDenoiseCS.hlsl's temporal blend + A-Trous, or
* DLSS Ray Reconstruction) is unaffected by this pass existing.
*
* ---------------------------------------------------------------------
*
* [JP]
* GI 用の ReSTIR 空間的リユース。GlobalIlluminationRayGeneration が全画素分の
* 今フレームの時間的結合済み Reservoir を書き終え、SRV として読めるバリアが
* 済んだ後に走る — 自分のピクセルと近傍数点の Reservoir を、全て今フレームの
* 書き込みスロット(constant_indices.global_illumination_.
* reservoir_write_srv_index_)から読み、GlobalIlluminationRayGeneration が
* 時間的履歴を畳み込むのと同じ要領でストリーミング結合する。近傍を(raygen 内で
* やる場合のように)前フレームのデータからではなく今フレーム自身のデータから
* 読むのが正しさの要: 深度/法線の妥当性判定と、実際に借りてくるサンプルが
* 同じ瞬間のものであることが保証されるため、ディスオクルージョン直後の画素は
* 「1フレーム前にそのピクセルにあった無関係なジオメトリへじわじわ寄っていく」
* のではなく、近傍の既に良質なサンプルをすぐ借りられる。
*
* 解決した放射輝度は、GlobalIlluminationRayGeneration が直接書いていたのと
* 同じ生テクスチャ(structured_indices.global_illumination_.output_uav_index_)
* へ書く — 後段(GlobalIlluminationDenoiseCS.hlsl の時間ブレンド+A-Trous、または
* DLSS Ray Reconstruction)はこのパスの有無を意識しない。
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

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	RWTexture2D<float4> output = ResourceDescriptorHeap[structured_indices.global_illumination_.output_uav_index_];

	if (depth == 0.0)
	{
		output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	ConstantBuffer<GlobalIlluminationRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.global_illumination_.ray_constant_index_];

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float3 normal = OctNormalDecode(normal_texture.Load(int3(pixel, 0)).rg);

	StructuredBuffer<GlobalIlluminationReservoir> reservoir_buffer = ResourceDescriptorHeap[constant_indices.global_illumination_.reservoir_write_srv_index_];
	GlobalIlluminationReservoir reservoir = reservoir_buffer[pixel.y * (uint)scene.screen_size_.x + pixel.x];

	// [JP] raygen とは違う定数オフセットを混ぜ、時間的リユースの乱数列と
	//      相関しないようにする(相関するとノイズが縞に見える)。
	uint rng_state = SeedFromPixel(pixel, tuning.frame_index_ + 2246822519u);

	int2 screen_max = int2(scene.screen_size_) - 1;
	float2 depth_gradient = DenoiserDepthGradient(depth_texture, int2(pixel), screen_max);

	// [JP] GI_RESERVOIR_M_CAP を下げた(時間方向の実効履歴を短くした)ぶんの
	//      ノイズ増を、時間方向ではなく空間方向のサンプル数で埋め合わせる -
	//      応答速度には影響しない(同一フレーム内の処理のため)。
	const uint SPATIAL_SAMPLE_COUNT = 8;
	const float SPATIAL_RADIUS = 24.0;
	const float SPATIAL_WEIGHT_THRESHOLD = 0.1;
	const float SPATIAL_DEPTH_SHARPNESS = 48.0;
	const float SPATIAL_NORMAL_POWER = 8.0;

	[unroll]
	for (uint sampleIndex = 0; sampleIndex < SPATIAL_SAMPLE_COUNT; sampleIndex++)
	{
		float angle = Rand(rng_state) * 6.28318530718;
		float radius = sqrt(Rand(rng_state)) * SPATIAL_RADIUS;
		int2 neighborOffset = int2(round(float2(cos(angle), sin(angle)) * radius));
		int2 neighborPixel = clamp(int2(pixel) + neighborOffset, int2(0, 0), screen_max);

		float neighborDepth = depth_texture.Load(int3(neighborPixel, 0));
		float3 neighborNormal = OctNormalDecode(normal_texture.Load(int3(neighborPixel, 0)).rg);
		float spatialWeight = DenoiserSpatialWeight(depth, depth_gradient, neighborOffset, neighborDepth, normal, neighborNormal, SPATIAL_DEPTH_SHARPNESS, SPATIAL_NORMAL_POWER);

		bool neighborValid = neighborDepth > 0.0 && spatialWeight > SPATIAL_WEIGHT_THRESHOLD;

		uint neighborIndex = (uint)neighborPixel.y * (uint)scene.screen_size_.x + (uint)neighborPixel.x;
		GlobalIlluminationReservoir neighbor = reservoir_buffer[neighborIndex];

		reservoir = GlobalIlluminationReservoirCombine(reservoir, neighbor, neighborValid, rng_state);
	}

	output[pixel] = float4(reservoir.sample_radiance_ * reservoir.sample_w_ * tuning.intensity_, 1.0);

	// [JP] このピクセルの reservoir が最終的にどれだけ収束しているか(時間的+
	//      空間的結合を経た後の M / 上限)を GlobalIlluminationDenoiseCS.hlsl
	//      へ渡す。ディスオクルージョン直後で M が低い間はデノイザ自身の
	//      時間的ブレンドに任せ、M が上限付近まで積み上がった定常状態では
	//      デノイザ側のブレンドをほぼバイパスする — reservoir と SVGF 相当の
	//      デノイザが【それぞれ独立に】長い時間平均を重ねる二重積分(体感的な
	//      「引きずられる」動きの原因)を避けるため。
	RWTexture2D<float> confidence_output = ResourceDescriptorHeap[structured_indices.global_illumination_.confidence_uav_index_];
	confidence_output[pixel] = saturate(reservoir.sample_m_ / GI_RESERVOIR_M_CAP);
}

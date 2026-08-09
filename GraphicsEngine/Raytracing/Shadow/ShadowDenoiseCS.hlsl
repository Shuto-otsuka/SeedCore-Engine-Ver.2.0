#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Denoiser.hlsli"
#include "Shadow.hlsli"

static const float SHADOW_TEMPORAL_BLEND_ALPHA = 0.05;

// [JP] 履歴を許容する範囲の幅(平均推定量の標準誤差の何倍まで許すか)。
//      大きいほど残像が残りやすく、小さいほどノイズが残る。2.0 は正規近似で
//      約95%の信頼区間に相当する。
static const float SHADOW_HISTORY_CLIP_SIGMA = 2.0;

static const float SHADOW_ATROUS_DEPTH_SHARPNESS = 48.0;
static const float SHADOW_ATROUS_NORMAL_POWER = 8.0;

/**
* [EN]
* Own (non-DLSS) temporal denoiser for the raw stochastic shadow signal.
* Reprojects the previous frame's accumulated result using the G-Buffer
* velocity buffer and exponentially blends it with this frame's raw sample,
* so the single noisy sample-per-pixel-per-frame from ShadowRT.hlsl converges
* to a smooth result over several frames instead of showing as static noise.
* If reprojection lands outside the screen (disocclusion, camera cut, first
* frame) history is discarded and this frame's raw sample is used as-is,
* naturally re-converging from there.
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

	// [JP] raw はビュー共有(structured_indices)、蓄積チェーンはビューごと
	// (constant_indices — Editor/Game で別バッファ)から取る。
	Texture2D<float2> raw_visibility = ResourceDescriptorHeap[structured_indices.shadow_.raw_visibility_srv_index_];
	RWTexture2D<float2> scratch_output = ResourceDescriptorHeap[constant_indices.shadow_.atrous_scratch0_uav_index_];

	float2 raw_value = raw_visibility.Load(int3(pixel, 0));

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		scratch_output[pixel] = float2(1.0, 1.0);
		return;
	}

	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;

	// [JP] velocity は (current_ndc - previous_ndc) * 0.5 で書き込まれている
	// (StaticModelPS.hlsl 等)。NDC->UV 変換で x は符号そのまま、y は反転する
	// ため、UV 空間の移動量は (velocity.x, -velocity.y) になる。
	Texture2D<float2> velocity_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_2_];
	float2 velocity = velocity_texture.Load(int3(pixel, 0));
	float2 delta_uv = float2(velocity.x, -velocity.y);
	float2 previous_uv = uv - delta_uv;

	float2 result;
	if (previous_uv.x >= 0.0 && previous_uv.x <= 1.0 && previous_uv.y >= 0.0 && previous_uv.y <= 1.0)
	{
		Texture2D<float2> history_visibility = ResourceDescriptorHeap[constant_indices.shadow_.history_srv_index_];
		float2 history_value = history_visibility.SampleLevel(sampler_linear_clamp, previous_uv, 0);

		// [JP] 近傍クランプ(TAAと同じ履歴棄却)。リプロジェクション先の履歴が
		// 「今この場所にある面」と無関係な値(カメラ移動によるディスオクルー
		// ジョン、初回フレームの未初期化値など)でも、範囲に押し込むことで
		// 残像・引きずりを防ぐ。
		//
		// 生値の min/max だけでは【半影で機能しない】。生値は 0/1 の二値なので
		// 半影の 3x3 にはほぼ必ず 0 と 1 が混在し、範囲が [0,1] = クランプが
		// 完全な no-op になる。つまり残像が実際に出る領域でだけ履歴棄却が
		// 効いていなかった。
		//
		// そこで min/max に加えて「平均推定量の信頼区間」も範囲として使う。
		// 単純な mean ± γσ は二値信号だと σ が最大 0.5 まで開いて結局 [0,1] に
		// 戻るので、σ ではなく【平均の標準誤差】で測る。さらに Agresti-Coull 風の
		// 擬似カウント(+2/+4)を入れて、近傍が全一致のときに標準誤差が 0 へ潰れて
		// 履歴を二値サンプルに固定してしまうのも防ぐ。
		// この推定量は生信号が二値であること(ShadowRT.hlsl は 0/1 しか書かない)
		// が前提。連続値になったら分散を実測する形へ変えること。
		float2 visibility_sum = float2(0.0, 0.0);
		float2 neighborhood_min = float2(1.0, 1.0);
		float2 neighborhood_max = float2(0.0, 0.0);
		int2 screen_max = int2(scene.screen_size_) - 1;

		[unroll]
		for (int dy = -1; dy <= 1; ++dy)
		{
			[unroll]
			for (int dx = -1; dx <= 1; ++dx)
			{
				int2 neighbor = clamp(int2(pixel) + int2(dx, dy), int2(0, 0), screen_max);
				float2 neighbor_value = raw_visibility.Load(int3(neighbor, 0));
				visibility_sum += neighbor_value;
				neighborhood_min = min(neighborhood_min, neighbor_value);
				neighborhood_max = max(neighborhood_max, neighbor_value);
			}
		}

		const float sample_count = 9.0;
		float2 mean = visibility_sum / sample_count;
		float2 adjusted_mean = (visibility_sum + 2.0) / (sample_count + 4.0);
		float2 standard_error = sqrt(adjusted_mean * (1.0 - adjusted_mean) / (sample_count + 4.0));
		float2 clip_radius = SHADOW_HISTORY_CLIP_SIGMA * standard_error;

		// [JP] 2つの範囲の【狭い方】を採る。近傍が全一致なら min/max が1点に
		//      潰れて即座に収束し(硬い影の境界での応答性は従来どおり)、半影では
		//      min/max が [0,1] になるので信頼区間側が効く。mean は必ず両方の
		//      範囲に含まれるので、下限が上限を追い越すことはない。
		float2 clip_min = max(mean - clip_radius, neighborhood_min);
		float2 clip_max = min(mean + clip_radius, neighborhood_max);

		history_value = clamp(history_value, clip_min, clip_max);
		result = lerp(history_value, raw_value, SHADOW_TEMPORAL_BLEND_ALPHA);
	}
	else
	{
		result = raw_value;
	}

	scratch_output[pixel] = result;
}

void AtrousPassCommon(uint2 pixel, Texture2D<float2> source, RWTexture2D<float2> dest, int step)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		dest[pixel] = float2(1.0, 1.0);
		return;
	}

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float3 center_normal = OctNormalDecode(normal_texture.Load(int3(pixel, 0)).rg);

	int2 screen_max = int2(scene.screen_size_) - 1;
	float2 depth_gradient = DenoiserDepthGradient(depth_texture, int2(pixel), screen_max);

	dest[pixel] = DenoiserATrousPass(source, depth_texture, normal_texture, int2(pixel), screen_max, depth, depth_gradient, center_normal, step, SHADOW_ATROUS_DEPTH_SHARPNESS, SHADOW_ATROUS_NORMAL_POWER);
}

[numthreads(8, 8, 1)]
void ATrousPass1(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();
	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	Texture2D<float2> source = ResourceDescriptorHeap[constant_indices.shadow_.atrous_scratch0_srv_index_];
	RWTexture2D<float2> dest = ResourceDescriptorHeap[constant_indices.shadow_.atrous_scratch1_uav_index_];
	AtrousPassCommon(dtid.xy, source, dest, 1);
}

[numthreads(8, 8, 1)]
void ATrousPass2(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();
	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	Texture2D<float2> source = ResourceDescriptorHeap[constant_indices.shadow_.atrous_scratch1_srv_index_];
	RWTexture2D<float2> dest = ResourceDescriptorHeap[constant_indices.shadow_.atrous_scratch0_uav_index_];
	AtrousPassCommon(dtid.xy, source, dest, 2);
}

[numthreads(8, 8, 1)]
void ATrousPass3(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();
	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	Texture2D<float2> source = ResourceDescriptorHeap[constant_indices.shadow_.atrous_scratch0_srv_index_];
	RWTexture2D<float2> dest = ResourceDescriptorHeap[constant_indices.shadow_.accumulated_uav_index_];
	AtrousPassCommon(dtid.xy, source, dest, 4);
}

#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Normal.hlsli"
#include "AmbientOcclusion.hlsli"

static const float AO_TEMPORAL_BLEND_ALPHA = 0.08;

// [JP] 履歴を許容する範囲の幅(平均推定量の標準誤差の何倍まで許すか)。
//      大きいほど残像が残りやすく、小さいほどノイズが残る。
static const float AO_HISTORY_CLIP_SIGMA = 2.0;

/**
* [EN]
* Spatio-temporal denoiser for the raw stochastic AO signal. First a 5x5
* depth-weighted bilateral average smooths the binary 1spp grain spatially
* (neighbors on a different surface get near-zero weight, so AO doesn't
* bleed across edges); then the previous frame's accumulated openness is
* reprojected via the G-Buffer velocity buffer, clamped to the 3x3 raw
* neighborhood min/max (rejects stale history from disocclusion / camera
* cuts / the uninitialized first frame) and exponentially blended toward the
* filtered sample.
*
* [JP]
* 生の確率的AO信号の空間+時間デノイザ。まず 5x5 の深度重み付きバイラテラル
* 平均で 1spp の二値ざらつきを空間的に均し(別の面にある近傍は重みほぼ0に
* なるのでエッジをAOが跨いで滲まない)、次に前フレームの蓄積開放度を
* G-Buffer の速度バッファでリプロジェクションし、生サンプル3x3近傍の
* min/max にクランプ(ディスオクルージョン・カメラカット・未初期化初回
* フレームの古い履歴を棄却)した上で、フィルタ済みサンプルへ指数ブレンド
* する。
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
	// (constant_indices — Editor/Game で別バッファ)から取る。
	Texture2D<float> raw_openness = ResourceDescriptorHeap[structured_indices.ambient_occlusion_.raw_srv_index_];
	RWTexture2D<float> accumulated_openness = ResourceDescriptorHeap[constant_indices.ambient_occlusion_.accumulated_uav_index_];

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		accumulated_openness[pixel] = 1.0;
		return;
	}

	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;

	// velocity は (current_ndc - previous_ndc) * 0.5 で書き込まれている
	// (StaticModelPS.hlsl 等)。NDC->UV 変換で y は反転するため、UV 空間の
	// 移動量は (velocity.x, -velocity.y)。
	Texture2D<float2> velocity_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_2_];
	float2 velocity = velocity_texture.Load(int3(pixel, 0));
	float2 delta_uv = float2(velocity.x, -velocity.y);
	float2 previous_uv = uv - delta_uv;

	// [JP] 空間フィルタ: 5x5 のバイラテラル平均で生の0/1ノイズを均してから
	//      時間積分する。重みは「平面フィット深度」+「法線の一致度」:
	//      単純な深度差だと、斜めから見た壁/床は同一平面なのに隣接ピクセル間の
	//      深度差が大きく、重みが潰れてフィルタが実質オフになりノイズが残る
	//      (グレージング角の面ほどざらざらだった原因)。局所の深度勾配から
	//      「同一平面なら期待される深度」を外挿し、そこからの逸脱で判定する。
	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float3 center_normal = OctNormalDecode(normal_texture.Load(int3(pixel, 0)).rg);

	int2 screen_max = int2(scene.screen_size_) - 1;

	// 局所深度勾配(中心差分)。同一平面上の傾き成分を表す。
	float depth_right = depth_texture.Load(int3(clamp(int2(pixel) + int2(1, 0), int2(0, 0), screen_max), 0));
	float depth_left = depth_texture.Load(int3(clamp(int2(pixel) + int2(-1, 0), int2(0, 0), screen_max), 0));
	float depth_down = depth_texture.Load(int3(clamp(int2(pixel) + int2(0, 1), int2(0, 0), screen_max), 0));
	float depth_up = depth_texture.Load(int3(clamp(int2(pixel) + int2(0, -1), int2(0, 0), screen_max), 0));
	float2 depth_gradient = float2(depth_right - depth_left, depth_down - depth_up) * 0.5;

	float weight_sum = 0.0;
	float weight_squared_sum = 0.0;
	float filtered_raw = 0.0;
	float neighborhood_min = 1.0;
	float neighborhood_max = 0.0;

	[unroll]
	for (int dy = -2; dy <= 2; ++dy)
	{
		[unroll]
		for (int dx = -2; dx <= 2; ++dx)
		{
			int2 neighbor = clamp(int2(pixel) + int2(dx, dy), int2(0, 0), screen_max);
			float neighbor_value = raw_openness.Load(int3(neighbor, 0));
			float neighbor_depth = depth_texture.Load(int3(neighbor, 0));

			// 平面フィット: 勾配から外挿した期待深度との差で「別の面」を判定。
			// 斜め見の壁でも同一平面なら期待深度に乗るので重みが保たれる。
			float expected_depth = depth + dot(depth_gradient, float2(dx, dy));
			float depth_difference = abs(neighbor_depth - expected_depth) / max(depth, 0.0001);
			float depth_weight = exp(-depth_difference * 48.0);

			// 法線の一致度: 深度が近くても向きの違う面(入隅の相手側など)は弾く。
			float3 neighbor_normal = OctNormalDecode(normal_texture.Load(int3(neighbor, 0)).rg);
			float normal_weight = pow(saturate(dot(center_normal, neighbor_normal)), 8.0);

			float weight = depth_weight * normal_weight;

			filtered_raw += neighbor_value * weight;
			weight_sum += weight;

			// 重みの二乗和。下の実効サンプル数(Kish)に使う。
			weight_squared_sum += weight * weight;

			// 近傍クランプの範囲は中心3x3のみ(広げすぎると履歴棄却が甘くなる)。
			if (abs(dx) <= 1 && abs(dy) <= 1)
			{
				neighborhood_min = min(neighborhood_min, neighbor_value);
				neighborhood_max = max(neighborhood_max, neighbor_value);
			}
		}
	}
	filtered_raw /= max(weight_sum, 0.0001);

	// [JP] 履歴の許容範囲。生値 3x3 の min/max だけでは【AO の中間調で機能しない】。
	//      生値は 0/1 の二値なので、遮蔽率が 0.1〜0.9 のあたりでは 3x3 にほぼ必ず
	//      0 と 1 が混在し、範囲が [0,1] = クランプが完全な no-op になる。
	//      つまり残像が実際に出る領域でだけ履歴棄却が効いていなかった。
	//
	//      そこで「平均推定量の信頼区間」を併用する。単純な mean ± γσ は二値信号
	//      だと σ が最大 0.5 まで開いて結局 [0,1] に戻るので、σ ではなく【平均の
	//      標準誤差】で測る。二値なので分散は平均から決まり、二次モーメントの
	//      蓄積は要らない。Agresti-Coull 風の擬似カウント(+2/+4)は、近傍が
	//      バイラテラル重みでほぼ棄却されて実効サンプル数が 1 に近い縁のピクセルで、
	//      標準誤差が 0 へ潰れて履歴を二値ノイズに固定してしまうのを防ぐ。
	//      この推定量は生信号が二値であること(AmbientOcclusionRT.hlsl は 0/1 しか
	//      書かない)が前提。連続値になったら分散を実測する形へ変えること。
	float effective_count = (weight_sum * weight_sum) / max(weight_squared_sum, 0.0001);
	effective_count = max(effective_count, 1.0);

	float adjusted_mean = (filtered_raw * effective_count + 2.0) / (effective_count + 4.0);
	float standard_error = sqrt(adjusted_mean * (1.0 - adjusted_mean) / (effective_count + 4.0));
	float clip_radius = AO_HISTORY_CLIP_SIGMA * standard_error;

	// [JP] 2つの範囲の【狭い方】を採る。近傍が全一致なら min/max が1点に潰れて
	//      即座に収束し(遮蔽の硬い境界での応答性は従来どおり)、中間調では
	//      min/max が [0,1] になるので信頼区間側が効く。
	//
	//      min/max は中心 3x3、filtered_raw は 5x5 の加重平均と【窓の幅が違う】
	//      ため、外周に別の値があると filtered_raw が 3x3 の範囲外へ出る。その
	//      まま交差を取ると下限が上限を追い越し、clamp が黙って上限を返す
	//      (= 履歴を無関係な値へ固定する)ので、min/max 側を filtered_raw を
	//      含むまで広げてから交差させる。通常ケースでは何も変わらない。
	float band_min = min(neighborhood_min, filtered_raw);
	float band_max = max(neighborhood_max, filtered_raw);

	float clip_min = max(filtered_raw - clip_radius, band_min);
	float clip_max = min(filtered_raw + clip_radius, band_max);

	float result;
	if (previous_uv.x >= 0.0 && previous_uv.x <= 1.0 && previous_uv.y >= 0.0 && previous_uv.y <= 1.0)
	{
		Texture2D<float> history_openness = ResourceDescriptorHeap[constant_indices.ambient_occlusion_.history_srv_index_];
		float history_value = history_openness.SampleLevel(sampler_linear_clamp, previous_uv, 0);

		history_value = clamp(history_value, clip_min, clip_max);
		result = lerp(history_value, filtered_raw, AO_TEMPORAL_BLEND_ALPHA);
	}
	else
	{
		result = filtered_raw;
	}

	accumulated_openness[pixel] = result;
}

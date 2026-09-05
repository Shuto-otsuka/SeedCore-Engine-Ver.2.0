#include "../Shader/Constants.hlsli"

/**
* [EN]
* Reference:
* - https://richardphotolab.com/blogs/post/film-grain-and-pixelation
* - https://www.alestemple.net/store/ofx-plugins/photochemical-film-grain-ofx-plugin.html
*
* Film grain. The part worth getting right is the luminance response: real
* film grain is strongest in the MIDTONES, not in the shadows and not in
* the highlights. In shadows few silver halide crystals were exposed, so
* there is little to see. In highlights so many were exposed that they pack
* together into continuous tone and individual grains stop being
* distinguishable. Only the midtones sit at the density where grains are
* both numerous and separable. Uniform noise across the whole tonal range
* is the usual tell that grain was faked, so the response below peaks at
* mid grey and falls off toward both ends.
*
* Grain size matters for the same reason: film grain is a clump of
* crystals, not a pixel. Per-pixel noise reads as digital sensor noise or
* video static, so the noise field is sampled at pixel coordinates divided
* by size_ to give grains actual extent.
*
* The seed advances with SceneConstantBuffer::total_time_ so the pattern is
* different every frame, the way film is a new piece of emulsion every
* frame. A static pattern reads as a dirty lens rather than grain.
*
* Colour film records grain per emulsion layer, so with colored_ set each
* channel gets its own independent value; monochrome grain is the cheaper
* approximation that shifts only brightness.
*
* Runs LAST, after SharpnessCS.hlsl, and read-modify-writes that pass's
* output in place. In place is safe because this is a per-pixel operation
* with no neighbour taps (the same reason BokehCS.hlsl may read-modify-write
* depth of field's buffer). Last is deliberate: sharpening grain would
* amplify it into crawling speckle.
*
* ---------------------------------------------------------------------
*
* [JP]
* フィルムグレイン。正しくやる価値があるのは階調応答の部分: 実際のフィルム
* グレインが最も強く出るのは【中間調】で、シャドウでもハイライトでもない。
* シャドウは露光したハロゲン化銀結晶が少ないので見えるものが少ない。
* ハイライトは露光した結晶が多すぎて互いに詰まり、連続した階調に溶けて
* 個々の粒が判別できなくなる。粒が「多く、かつ分離して見える」濃度に居るのは
* 中間調だけ。階調全域に一様なノイズを乗せるのが「グレインが偽物」と分かる
* 典型的な兆候なので、下の応答は中間グレーで最大になり両端へ向かって落ちる。
*
* 粒の大きさが重要なのも同じ理由。フィルムグレインは結晶の塊であって画素
* ではない。画素単位のノイズはデジタルのセンサーノイズかテレビの砂嵐に
* 見えるので、ノイズ場は画素座標を size_ で割った位置でサンプルし、粒に
* 実際の大きさを持たせる。
*
* シードは SceneConstantBuffer::total_time_ で進めるので、毎フレーム違う
* 模様になる — フィルムが毎コマ別の乳剤であるのと同じ。静止した模様は
* グレインではなくレンズの汚れに見える。
*
* カラーフィルムは乳剤層ごとに粒を持つので、colored_ が立っていれば
* チャンネルごとに独立した値を与える。モノクロのグレインは明るさだけを
* 動かす簡略版。
*
* SharpnessCS.hlsl の後、最後に走り、その出力をその場で read-modify-write
* する。その場で安全なのは近傍タップの無い画素ごとの処理だから
* (BokehCS.hlsl が被写界深度のバッファを read-modify-write できるのと
* 同じ理由)。最後に置くのは意図的で、グレインをシャープにすると
* 這い回るスペックルへ増幅されてしまう。
*/

/**
* [EN]
* Hash that builds one [0,1) pseudo-random value from a coordinate and a
* seed.
*
* ---------------------------------------------------------------------
*
* [JP]
* 座標とシードから [0,1) の擬似乱数を1つ作るハッシュ。
*/
float GrainHash(float2 position, float seed)
{
	return frac(sin(dot(position, float2(12.9898, 78.233)) + seed) * 43758.5453);
}

/**
* [EN]
* Film's luminance response. 1 at mid grey, falling to 0 at both black and
* white. 4*l*(1-l) is exactly that shaped parabola, peaking at 1 when
* l=0.5. Grain is invisible in shadows because few crystals were exposed,
* and invisible in highlights because too many crystals pack together into
* continuous tone - both ends falling to 0 expresses those two separate
* reasons with a single curve.
*
* ---------------------------------------------------------------------
*
* [JP]
* フィルムの階調応答。中間グレーで1になり、黒と白の両端で0へ落ちる。
* 4*l*(1-l) はちょうどその形の放物線で、l=0.5 で最大値1を取る。シャドウで
* 粒が見えないのは露光した結晶が少ないから、ハイライトで見えないのは結晶が
* 詰まりすぎて連続階調に溶けるから - 両端が0へ落ちるのはその2つの別々の
* 理由を1本の曲線で表している。
*/
float GrainLuminanceResponse(float luminance, float response)
{
	float midtone_weight = 4.0 * luminance * (1.0 - luminance);
	return lerp(1.0, saturate(midtone_weight), saturate(response));
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> destination = ResourceDescriptorHeap[constant_indices.post_process_.film_grain_.destination_uav_index_];

	uint width, height;
	destination.GetDimensions(width, height);

	/// [EN] Bounds guard: the dispatch is rounded up to a multiple of the
	///      8x8 thread group size, so threads past the actual screen edge
	///      must bail out before touching any resource.
	/// [JP] 範囲外ガード: ディスパッチは 8x8 スレッドグループの倍数に
	///      切り上げられているので、実際の画面端を超えたスレッドはどの
	///      リソースにも触れる前に抜ける必要がある。
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	float3 color = destination[dtid.xy].rgb;

	float intensity = constant_indices.post_process_.film_grain_.intensity_;
	float size = max(constant_indices.post_process_.film_grain_.size_, 1.0);
	float response = constant_indices.post_process_.film_grain_.luminance_response_;
	bool colored = constant_indices.post_process_.film_grain_.colored_ != 0;

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	float seed = scene.total_time_;

	/// [EN] Grain size comes from sampling noise on a grid of pixel
	///      coordinates divided by size. Flooring means every group of size
	///      pixels shares the same random value, forming a clump rather than
	///      per-pixel noise.
	/// [JP] 粒の大きさは、画素座標を size で割った格子でノイズを引くことで
	///      出す。floor しているので size 画素ぶんが同じ乱数値を共有し、
	///      画素単位ではない塊になる。
	float2 grain_position = floor(float2(dtid.xy) / size);

	float3 grain;
	if (colored)
	{
		grain.r = GrainHash(grain_position, seed);
		grain.g = GrainHash(grain_position, seed + 17.0);
		grain.b = GrainHash(grain_position, seed + 43.0);
	}
	else
	{
		grain = GrainHash(grain_position, seed).xxx;
	}

	/// [EN] Maps the random value to [-1,1], scales it by the luminance
	///      response and intensity, then adds it. Adding (rather than
	///      multiplying) matches the physics: the emulsion's density
	///      variation is a variance in the density itself, not a multiplier
	///      proportional to the original brightness.
	/// [JP] 乱数を [-1,1] へ写して、階調応答と強度で調整してから加算する。
	///      加算(乗算ではなく)なのは、乳剤の濃度ムラが元の明るさに比例した
	///      倍率ではなく、濃度そのもののばらつきだから。
	float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
	float weight = GrainLuminanceResponse(saturate(luminance), response) * intensity;

	destination[dtid.xy] = float4(saturate(color + (grain * 2.0 - 1.0) * weight), 1.0);
}

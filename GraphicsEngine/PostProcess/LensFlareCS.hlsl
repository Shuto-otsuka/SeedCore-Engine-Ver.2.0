#include "../Shader/Constants.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [EN]
* Reference:
* - https://john-chapman.github.io/2017/11/05/pseudo-lens-flare.html
* - https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare/
*
* Aperture diffraction-spike ("starburst") lens flare, built as a
* multi-pass Kawase-style directional blur (the same technique described in
* Call of Duty: Advanced Warfare's next-gen post-processing talk) instead of
* a single-dispatch sparse gather.
*
* The spike pattern follows the physics of an n-bladed iris: each blade
* edge diffracts light perpendicular to itself, and one edge produces a
* single LINE (two opposing arms 180 degrees apart), so an even-bladed iris
* shows n spikes - opposing edges are parallel and their lines coincide -
* while an odd-bladed one shows 2n. That makes the axis count n/2 for even
* n and n for odd n; PostProcessRenderer::PrepareView derives it from
* BokehSettings::bladeCount_ (the same physical iris the bokeh shape comes
* from) into axis_count_, and the axes are spread evenly over half a turn.
* Each axis is blurred independently and bidirectionally across 4 passes
* (BlurPass1..4), ping-ponging between that axis's two dedicated buffers
* (LensFlareStreakIndices).
*
* Each pass takes 3 taps per side at a texel spacing of 4^(pass-1)
* (1, 4, 16, 64) and weights a tap `d` texels out by
* streak_attenuation_^d - Kawase's a^(b*s) formula. That exponential decay
* IS the spike's falloff: it is what makes an arm taper and fade toward its
* tip. An equal-weight box average over the same taps produces no falloff
* at all, so every arm stays as bright at its tip as at its root and reads
* as a hard solid bar rather than light. The per-pass result is normalized
* by the weight sum, because without that each pass multiplies brightness
* by up to 7 and four passes blow out to a saturated white cross;
* intensity_ is the knob for overall brightness, not the kernel gain.
*
* Because each pass reads the PREVIOUS pass's already-filtered result
* rather than the raw HDR source, the streak comes out continuous even when
* the underlying bright source is large (e.g. a close point light's soft
* specular highlight) - a single-dispatch sparse gather would instead
* reproduce separate copies of that source at each sample position, visible
* as a chain of overlapping circles ("beads on a string").
* BlurPass1 also performs the bright-pass extraction (from the same HDR
* source ToneMappingCS.hlsl reads); Compose reads each axis's final
* (post-pass-6) buffer, applies the color-fringing chromatic_aberration_
* offset and intensity_ once, and writes the summed result directly into
* LensFlareIndices' output texture that ToneMappingCS.hlsl samples and adds
* into the HDR color before exposure. A final Ghost pass adds the classic
* "ghost chain" - bright-passed copies of the source strung out through
* screen center to the opposite side, fading with distance from screen
* center and with ghost index - on top of the streak result via a single
* gather dispatch that read-modify-writes the same output texture.
*
* ---------------------------------------------------------------------
*
* [JP]
* 絞りの回折スパイク(スターバースト)型レンズフレア。1回のディスパッチで
* 疎にゲザーする方式ではなく、多段階Kawase式ディレクショナルブラー
* (Call of Duty: Advanced Warfareの次世代ポストプロセス講演で知られる手法)
* として構築する。
*
* 棘の並びは羽根n枚の絞りの物理に従う: 各羽根のエッジはそれ自身に垂直な
* 方向へ回折し、1つのエッジが【直線1本】(180°対向する腕2本)を作る。
* よって偶数枚の絞りでは棘がn本にしか見えず(向かい合うエッジが平行で
* 直線が重なるため)、奇数枚では2n本になる。軸数は偶数で n/2、奇数で n。
* PostProcessRenderer::PrepareView が BokehSettings::bladeCount_
* (ボケの形を決めるのと同じ物理的な絞り)から計算して axis_count_ に
* 入れ、軸は半周に等間隔で並ぶ。各軸を独立に、4パス(BlurPass1..4)
* かけて双方向にブラーする。その軸専用の2枚のバッファ
* (LensFlareStreakIndices)間でピンポンする。
*
* 各パスは片側3タップずつを 4^(パス番号-1) テクセル間隔(1, 4, 16, 64)で
* 取り、d テクセル先のタップの重みを streak_attenuation_^d にする —
* Kawase の a^(b*s) の式そのもの。この指数減衰が【棘の減衰そのもの】で、
* 腕が先端へ向かって細く暗くなるのはこれによる。同じタップを等重みで
* 箱平均すると減衰が一切起きず、どの腕も先端まで根元と同じ明るさの
* 硬いバーになって光に見えない。パスごとの結果は重みの合計で正規化する。
* しないと1パスあたり最大7倍のゲインが掛かり、4パスで飽和した白い十字に
* なる。全体の明るさを決めるのはカーネルのゲインではなく intensity_。
*
* 各パスが生のHDRソースではなく【前パスの既にフィルタ済みの結果】を
* 読むため、元の明るい光源が大きい場合(近距離のポイントライトの柔らかい
* スペキュラハイライトなど)でも連続した線になる — 1回ディスパッチの
* 疎なゲザーは、少数のサンプル位置ごとにその光源の独立したコピーを
* 複製してしまい、重なり合った円の数珠つなぎ(ビーズ状)に見えていた。
* BlurPass1はブライトパス抽出も兼ねる
* (ToneMappingCS.hlsl が読むのと同じHDRソースから)。Compose は各軸の
* 最終(パス4後)バッファを読み、色収差(chromatic_aberration_)オフセットと
* intensity_ を1回だけ適用し、合計結果を LensFlareIndices の出力
* テクスチャ(ToneMappingCS.hlsl がサンプルして露出適用前のHDRカラーへ
* 加算する)へ直接書き込む。最後に Ghost パスが古典的な「ゴーストチェーン」
* (画面中心を通り抜けて反対側まで連なる光源のブライトパス済みコピー、
* 画面中心からの距離とゴーストのインデックスに応じてフェードする)を
* ストリークの結果へ加算する — 1回のゲザーディスパッチで同じ出力
* テクスチャを読み書きする。
*/

/**
* [EN]
* Soft threshold weighting used before the bright-pass extraction below (the
* Downsample pass). Fades to 0 as luminance approaches threshold from above,
* rather than a hard cutoff.
*
* ---------------------------------------------------------------------
*
* [JP]
* 下のブライトパス抽出(Downsampleパス)前に使うソフトなしきい値重み。
* 硬い切り捨てではなく、輝度が threshold へ上から近づくにつれて0へ
* フェードする。
*/
float3 BrightPass(float3 color, float threshold)
{
	float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
	float weight = max(luminance - threshold, 0.0) / max(luminance, 0.0001);
	return color * weight;
}

/**
* [EN]
* Direction of a spike's axis. One axis is "a single line = two opposing
* arms", so the axes are spread evenly over half a turn (pi). This follows
* directly from an iris's diffraction physics: each blade edge diffracts
* perpendicular to itself, so an n-bladed iris shows n spikes when n is
* even, 2n when n is odd - axis counts of n/2 and n respectively. The C++
* side (PostProcessRenderer::PrepareView) computes axis_count_ from
* BokehSettings::bladeCount_. A 6-bladed iris gives 3 axes 60 degrees apart,
* matching the value that used to be hardcoded.
*
* ---------------------------------------------------------------------
*
* [JP]
* 棘の軸の向き。軸1本が「直線1本 = 対向する腕2本」なので、軸を半周(π)に
* 等間隔で並べる。これは絞りの回折そのもので、羽根の各エッジがそれ自身に
* 垂直な方向へ回折し、羽根n枚なら偶数でn本、奇数で2n本の棘になる - 軸数は
* それぞれ n/2、n。軸数はC++側(PostProcessRenderer::PrepareView)が
* BokehSettings::bladeCount_ から計算して axis_count_ に入れ、羽根6枚なら
* 3軸60°間隔で、これは以前ハードコードされていた値と一致する。
*/
float2 AxisDirection(uint axis, float angle_offset, uint axis_count)
{
	float angle = angle_offset + (3.14159265 / float(axis_count)) * float(axis);
	return float2(cos(angle), sin(angle));
}

uint AxisPingUnorderedAccessViewIndex(uint axis)
{
	return constant_indices.post_process_.lens_flare_streak_.axis_indices_[axis].x;
}

uint AxisPingShaderResourceViewIndex(uint axis)
{
	return constant_indices.post_process_.lens_flare_streak_.axis_indices_[axis].y;
}

uint AxisPongUnorderedAccessViewIndex(uint axis)
{
	return constant_indices.post_process_.lens_flare_streak_.axis_indices_[axis].z;
}

uint AxisPongShaderResourceViewIndex(uint axis)
{
	return constant_indices.post_process_.lens_flare_streak_.axis_indices_[axis].w;
}

/**
* [EN]
* The final result after the 4th pass always lands in the pong buffer (since
* passes 2 and 4 write to pong). Compose only ever needs to read this one.
*
* ---------------------------------------------------------------------
*
* [JP]
* 4パス目の最終結果は常に pong 側に着地する(2/4パス目が pong へ書くため)。
* Compose はここだけを読めばよい。
*/
uint AxisFinalShaderResourceViewIndex(uint axis)
{
	return AxisPongShaderResourceViewIndex(axis);
}

/**
* [EN]
* Per-spike variation coefficient. Built deterministically from the axis
* index, so it stays stable frame to frame instead of flickering over time.
* With variation at 0 this always returns 1.0, giving an ideally symmetric
* star. This is stylization, not physics - a perfect iris is fully
* symmetric; a real one is uneven because of manufacturing tolerance and
* assembly misalignment of the blades.
*
* ---------------------------------------------------------------------
*
* [JP]
* 棘ごとのばらつき係数。軸番号から決定論的に作るのでフレーム間で安定し、
* 時間的なちらつきにならない。variation が0なら常に1.0を返して理想的な
* 対称の星になる。物理ではなく演出 - 理想的な絞りは完全に対称で、実物が
* 不揃いなのは羽根の製造誤差と組み付けのずれ。
*/
float AxisVariation(uint axis, float variation, float phase)
{
	float noise = frac(sin(float(axis) * 12.9898 + phase) * 43758.5453);
	return 1.0 - variation * noise;
}

/**
* [EN]
* Downsample: builds the input buffer every lens flare pass shares. Bright-
* pass extracts while dropping the full-resolution HDR source (or DoF
* output) to 1/4 resolution. Placing the 4 bilinear taps at source texel +-1
* makes each tap average its own 2x2, so the 4 taps together cover a full
* 4x4 source footprint - naively sampling mip 0 at 1/4 resolution instead
* would skip 15 of every 16 pixels, and a light source only a few pixels
* wide (a sun disc, for instance) would vanish entirely (the cause of "no
* flare on the sun"). The bright pass is applied to EACH tap BEFORE
* averaging - applying it after would let a small light source get diluted
* by the average first and then disappear below the threshold.
*
* ---------------------------------------------------------------------
*
* [JP]
* Downsample: 全レンズフレアパスが共有する入力バッファを作る。フル解像度の
* HDRソース(またはDOF出力)をブライトパス抽出しながら1/4解像度へ落とす。
* 4タップのバイリニアサンプルをソーステクセル±1に置くことで、各タップが
* 2x2を平均し、4タップ合計で4x4のソース範囲を丸ごとカバーする - 単純に
* 1/4解像度でmip 0をサンプルすると16画素中15画素を読み飛ばすため、太陽の
* 円盤のような数画素幅の光源が丸ごと消えてしまう(これが「太陽にフレアが
* 出ない」原因)。ブライトパスはタップごとに平均の【前】に掛ける - 後に
* 掛けると小さな光源が平均で薄まった後にしきい値で消えてしまうため。
*/
[numthreads(8, 8, 1)]
void Downsample(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> destination = ResourceDescriptorHeap[constant_indices.post_process_.lens_flare_streak_.bright_uav_index_];

	uint width, height;
	destination.GetDimensions(width, height);

	/// [EN] Bounds guard: the dispatch is rounded up to a multiple of the
	///      8x8 thread group size, so threads past the actual (1/4-res)
	///      output edge must bail out before touching any resource.
	/// [JP] 範囲外ガード: ディスパッチは 8x8 スレッドグループの倍数に
	///      切り上げられているので、実際の(1/4解像度)出力端を超えた
	///      スレッドはどのリソースにも触れる前に抜ける必要がある。
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	uint source_index = constant_indices.post_process_.depth_of_field_.enabled_ != 0 ? constant_indices.post_process_.depth_of_field_.shader_resource_view_index_ : constant_indices.post_process_.source_color_index_;
	Texture2D<float4> source = ResourceDescriptorHeap[source_index];

	uint source_width, source_height;
	source.GetDimensions(source_width, source_height);
	float2 source_texel = 1.0 / float2(source_width, source_height);

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);
	float threshold = constant_indices.post_process_.lens_flare_.threshold_;

	float3 result = BrightPass(source.SampleLevel(sampler_linear_clamp, uv + float2(-1.0, -1.0) * source_texel, 0).rgb, threshold);
	result += BrightPass(source.SampleLevel(sampler_linear_clamp, uv + float2(1.0, -1.0) * source_texel, 0).rgb, threshold);
	result += BrightPass(source.SampleLevel(sampler_linear_clamp, uv + float2(-1.0, 1.0) * source_texel, 0).rgb, threshold);
	result += BrightPass(source.SampleLevel(sampler_linear_clamp, uv + float2(1.0, 1.0) * source_texel, 0).rgb, threshold);

	destination[dtid.xy] = float4(result * 0.25, 1.0);
}

/**
* [EN]
* The core Kawase light-streak kernel. Takes the center tap plus 3 taps on
* each side along the axis (7 taps total), weighted by
* attenuation^(texel distance) - plain exponential decay with distance. This
* is why a spike tapers and darkens toward its tip; an equal-weight box
* average would produce no decay at all, leaving a hard bar equally bright
* from root to tip (which is what actually happened before this).
*
* spacing is the texel interval, quadrupling per pass (1, 4, 16, 64). Since
* the weight is exponential in distance, a pass with wider spacing has its
* side taps' contribution shrink much faster, building a long streak in few
* passes while the tip still fades out naturally.
*
* Normalized by dividing by the weight sum. Without this each pass would
* apply up to 7x gain, reaching thousands of times over 4 passes and
* saturating. Overall brightness is intensity_'s job instead.
*
* ---------------------------------------------------------------------
*
* [JP]
* Kawaseのライトストリークのカーネル本体。中心1タップ + 軸方向の両側3
* タップずつの計7タップを取り、重みは attenuation^(テクセル距離) - 距離に
* 対する指数減衰そのもの。これが棘が先端へ向かって細く暗くなる理由で、
* 等重みの箱型平均だと減衰が一切起きず、根元から先端まで同じ明るさの
* 硬いバーになってしまう(実際そうなっていた)。
*
* spacing はパスごとに4倍になるテクセル間隔(1, 4, 16, 64)。重みが距離の
* 指数なので、間隔が広いパスほど側タップの寄与が急速に小さくなり、少ない
* パス数で長い筋を作りつつ先端が自然に消える。
*
* 重みの合計で割って正規化する。割らないとパスごとに最大7倍のゲインが
* 掛かり、4パスで数千倍になって飽和する。明るさの調整は intensity_ の役目。
*/
float3 StreakGather(Texture2D<float4> source, float2 uv, float2 step_vector, float spacing, float attenuation)
{
	/// [EN] The attenuation must always be kept below 1. If it exceeds 1,
	///      pow becomes exponential 【growth】 instead of decay, and since
	///      the texel distance reaches up to 64*3 = 192, pow(1.5, 192)
	///      alone hits Inf. This function's final sum / weight_sum then
	///      returns Inf/Inf = NaN, which propagates through 4 passes, from
	///      Compose into the lens flare buffer, and spreads across the
	///      whole screen when ToneMappingCS.hlsl adds it into hdr_color -
	///      turning the frame solid black. The CPU-side slider range
	///      (0.80-0.99) cannot be trusted - a scene saved before that range
	///      changed can still carry an old-range value (up to 8.0), and this
	///      actually produced a black screen in practice.
	/// [JP] 減衰は必ず1未満に抑える。1を超えると pow が減衰ではなく指数
	///      【増加】になり、テクセル距離が最大 64*3 = 192 まで伸びるため
	///      pow(1.5, 192) の時点で Inf になる。するとこの関数の最後の
	///      sum / weight_sum が Inf/Inf = NaN を返し、NaN が4パス伝播して
	///      Compose からレンズフレアバッファへ、さらに ToneMappingCS.hlsl の
	///      hdr_color への加算で画面全体へ広がって真っ黒になる。CPU側の
	///      スライダー範囲(0.80-0.99)を信用してはいけない - レンジを
	///      変更する前に保存されたシーンには旧レンジの値(最大8.0)が
	///      そのまま入っており、実際にこれで黒画面になった。
	float safe_attenuation = clamp(attenuation, 0.5, 0.999);

	float3 sum = source.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
	float weight_sum = 1.0;

	[unroll]
	for (uint tap = 1; tap <= 3; tap++)
	{
		float texel_distance = spacing * float(tap);
		float weight = pow(safe_attenuation, texel_distance);
		float2 offset = step_vector * texel_distance;

		sum += source.SampleLevel(sampler_linear_clamp, uv + offset, 0).rgb * weight;
		sum += source.SampleLevel(sampler_linear_clamp, uv - offset, 0).rgb * weight;
		weight_sum += weight * 2.0;
	}

	return sum / weight_sum;
}

/**
* [EN]
* BlurPass1: reads the bright-passed buffer Downsample wrote and, per axis,
* applies the streak kernel and writes to the ping buffer (the threshold was
* already applied by Downsample, so it is not applied again here).
*
* ---------------------------------------------------------------------
*
* [JP]
* BlurPass1: Downsample が書いたブライトパス済みバッファを読み、軸ごとに
* ストリークのカーネルを掛けて ping バッファへ書く(しきい値は Downsample
* で適用済みなのでここでは掛けない)。
*/
void FirstPass(uint3 dtid, float spacing)
{
	RWTexture2D<float4> dims_reference = ResourceDescriptorHeap[AxisPingUnorderedAccessViewIndex(0)];
	uint width, height;
	dims_reference.GetDimensions(width, height);

	/// [EN] Bounds guard: the dispatch is rounded up to a multiple of the
	///      8x8 thread group size, so threads past the actual buffer edge
	///      must bail out before touching any resource.
	/// [JP] 範囲外ガード: ディスパッチは 8x8 スレッドグループの倍数に
	///      切り上げられているので、実際のバッファ端を超えたスレッドは
	///      どのリソースにも触れる前に抜ける必要がある。
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);
	float2 texel = 1.0 / float2(width, height);
	float angle_offset = constant_indices.post_process_.lens_flare_.angle_offset_;
	float attenuation = constant_indices.post_process_.lens_flare_.streak_attenuation_;
	float length_scale = constant_indices.post_process_.lens_flare_.streak_length_ * 4.0;
	uint axis_count = constant_indices.post_process_.lens_flare_.axis_count_;
	float variation = constant_indices.post_process_.lens_flare_.spike_variation_;

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.post_process_.lens_flare_streak_.bright_srv_index_];

	[loop]
	for (uint axis = 0; axis < axis_count; axis++)
	{
		float2 step_vector = AxisDirection(axis, angle_offset, axis_count) * texel * length_scale;
		float axis_attenuation = attenuation * AxisVariation(axis, variation * 0.15, 0.0);
		float3 result = StreakGather(source, uv, step_vector, spacing, axis_attenuation);

		RWTexture2D<float4> dest = ResourceDescriptorHeap[AxisPingUnorderedAccessViewIndex(axis)];
		dest[dtid.xy] = float4(result, 1.0);
	}
}

/**
* [EN]
* Shared body for BlurPass2..4: when read_ping is true, reads ping and
* writes to pong; when false, reads pong and writes to ping (fixed at
* compile time per pass).
*
* ---------------------------------------------------------------------
*
* [JP]
* BlurPass2..4共通本体: read_ping が true なら ping を読んで pong へ、
* false なら pong を読んで ping へ書く(パスごとにコンパイル時固定)。
*/
void BlurPass(uint3 dtid, float spacing, bool read_ping)
{
	RWTexture2D<float4> dims_reference = ResourceDescriptorHeap[AxisPingUnorderedAccessViewIndex(0)];
	uint width, height;
	dims_reference.GetDimensions(width, height);

	/// [EN] Bounds guard: the dispatch is rounded up to a multiple of the
	///      8x8 thread group size, so threads past the actual buffer edge
	///      must bail out before touching any resource.
	/// [JP] 範囲外ガード: ディスパッチは 8x8 スレッドグループの倍数に
	///      切り上げられているので、実際のバッファ端を超えたスレッドは
	///      どのリソースにも触れる前に抜ける必要がある。
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);
	float2 texel = 1.0 / float2(width, height);
	float angle_offset = constant_indices.post_process_.lens_flare_.angle_offset_;
	float attenuation = constant_indices.post_process_.lens_flare_.streak_attenuation_;
	float length_scale = constant_indices.post_process_.lens_flare_.streak_length_ * 4.0;
	uint axis_count = constant_indices.post_process_.lens_flare_.axis_count_;
	float variation = constant_indices.post_process_.lens_flare_.spike_variation_;

	[loop]
	for (uint axis = 0; axis < axis_count; axis++)
	{
		float2 step_vector = AxisDirection(axis, angle_offset, axis_count) * texel * length_scale;
		float axis_attenuation = attenuation * AxisVariation(axis, variation * 0.15, 0.0);

		uint source_index = read_ping ? AxisPingShaderResourceViewIndex(axis) : AxisPongShaderResourceViewIndex(axis);
		Texture2D<float4> source = ResourceDescriptorHeap[source_index];

		float3 result = StreakGather(source, uv, step_vector, spacing, axis_attenuation);

		uint destination_index = read_ping ? AxisPongUnorderedAccessViewIndex(axis) : AxisPingUnorderedAccessViewIndex(axis);
		RWTexture2D<float4> dest = ResourceDescriptorHeap[destination_index];
		dest[dtid.xy] = float4(result, 1.0);
	}
}

/**
* [EN]
* Per-pass texel spacing is 4^(pass number - 1) = 1, 4, 16, 64 - the
* multiplier Kawase's light streak defines. With 3 taps per side per pass,
* the reach totals 3 + 12 + 48 + 192 = 255 texels (over half the screen,
* since the working resolution is 1/4 native). The 4x multiplier needs fewer
* passes than a 2x step would, which is what let this drop from 6 passes to
* 4.
*
* ---------------------------------------------------------------------
*
* [JP]
* パスごとのテクセル間隔は 4^(パス番号-1) = 1, 4, 16, 64。Kawase の
* ライトストリークが定める倍率で、1パスあたり片側3タップなので到達距離は
* 3 + 12 + 48 + 192 = 255 テクセル(作業解像度がネイティブの1/4なので画面の
* 半分以上)に届く。倍率が4なぶん2倍刻みより少ないパス数で済み、以前の6
* パスから4パスへ減らせた。
*/
[numthreads(8, 8, 1)]
void BlurPass1(uint3 dtid : SV_DispatchThreadID)
{
	FirstPass(dtid, 1.0);
}

[numthreads(8, 8, 1)]
void BlurPass2(uint3 dtid : SV_DispatchThreadID)
{
	BlurPass(dtid, 4.0, true);
}

[numthreads(8, 8, 1)]
void BlurPass3(uint3 dtid : SV_DispatchThreadID)
{
	BlurPass(dtid, 16.0, false);
}

[numthreads(8, 8, 1)]
void BlurPass4(uint3 dtid : SV_DispatchThreadID)
{
	BlurPass(dtid, 64.0, true);
}

[numthreads(8, 8, 1)]
void Compose(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> output = ResourceDescriptorHeap[constant_indices.post_process_.lens_flare_.unordered_access_view_index_];

	uint width, height;
	output.GetDimensions(width, height);

	/// [EN] Bounds guard: the dispatch is rounded up to a multiple of the
	///      8x8 thread group size, so threads past the actual output edge
	///      must bail out before touching any resource.
	/// [JP] 範囲外ガード: ディスパッチは 8x8 スレッドグループの倍数に
	///      切り上げられているので、実際の出力端を超えたスレッドはどの
	///      リソースにも触れる前に抜ける必要がある。
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);
	float chromatic_aberration = constant_indices.post_process_.lens_flare_.chromatic_aberration_;
	float intensity = constant_indices.post_process_.lens_flare_.intensity_;
	float angle_offset = constant_indices.post_process_.lens_flare_.angle_offset_;
	uint axis_count = constant_indices.post_process_.lens_flare_.axis_count_;
	float variation = constant_indices.post_process_.lens_flare_.spike_variation_;

	float3 accum = float3(0.0, 0.0, 0.0);

	[loop]
	for (uint axis = 0; axis < axis_count; axis++)
	{
		Texture2D<float4> axis_source = ResourceDescriptorHeap[AxisFinalShaderResourceViewIndex(axis)];
		float2 direction = AxisDirection(axis, angle_offset, axis_count);

		/// [EN] Chromatic aberration comes from sampling R/G/B at different
		///      positions along the spike. Diffraction angle is
		///      proportional to wavelength (longer wavelengths spread more),
		///      so red shifting outward and blue inward is physically the
		///      right direction.
		/// [JP] 色収差は棘に沿ってR/G/Bを別位置からサンプルすることで出す。
		///      回折角は波長に比例する(長波長ほど大きく広がる)ので、
		///      赤が外側・青が内側にずれるのは方向として物理どおり。
		float2 fringe = direction * chromatic_aberration;

		float r = axis_source.SampleLevel(sampler_linear_clamp, uv + fringe, 0).r;
		float g = axis_source.SampleLevel(sampler_linear_clamp, uv, 0).g;
		float b = axis_source.SampleLevel(sampler_linear_clamp, uv - fringe, 0).b;

		accum += float3(r, g, b) * AxisVariation(axis, variation, 7.0);
	}

	output[dtid.xy] = float4(accum * intensity, 1.0);
}

/**
* [EN]
* Samples color shifted radially (chromatic aberration). Reading R/G/B each
* with a different offset along direction produces the rainbow fringe at the
* edge of a ghost or halo.
*
* ---------------------------------------------------------------------
*
* [JP]
* 半径方向に色をずらしてサンプルする(色収差)。R/G/Bをそれぞれ direction
* に沿った別のオフセットで読むことで、ゴーストとハローの縁に虹色の
* フリンジが出る。
*/
float3 SampleDistorted(Texture2D<float4> source, float2 uv, float2 direction, float3 distortion)
{
	float r = source.SampleLevel(sampler_linear_wrap, uv + direction * distortion.r, 0).r;
	float g = source.SampleLevel(sampler_linear_wrap, uv + direction * distortion.g, 0).g;
	float b = source.SampleLevel(sampler_linear_wrap, uv + direction * distortion.b, 0).b;
	return float3(r, g, b);
}

/**
* [EN]
* Reference:
* - https://john-chapman.github.io/2017/11/05/pseudo-lens-flare.html
*   (John Chapman, "Pseudo Lens Flare" - the ghost-chain + halo construction
*   the Ghost pass below follows.)
*
* Lens color gradient looked up by normalized radius from screen center. The
* canonical implementation samples a 1D LUT texture, but this substitutes a
* phase-shifted cosine palette instead to avoid adding an asset - each ghost
* changing color is the single biggest factor in a lens flare reading as
* "natural", a very different look from adding everything in plain white.
*
* ---------------------------------------------------------------------
*
* [JP]
* 画面中心からの正規化半径で引くレンズカラーのグラデーション。本来の実装は
* 1DのLUTテクスチャを引くが、アセットを増やさずに済むよう位相をずらした
* コサインパレットで代用する - ゴーストごとに色が変わるのがレンズフレアが
* 「自然」に見える一番の要因なので、白のまま加算するのとは見た目が大きく
* 変わる。
*/
float3 LensColor(float radius01)
{
	return 0.6 + 0.4 * cos(6.28318530718 * (radius01 + float3(0.0, 0.33, 0.67)));
}

/**
* [EN]
* Ghost: a chain of bokeh circles (ghosts) strung out to the opposite side
* across screen center, plus a halo (ring) around them. Follows John
* Chapman's "Pseudo Lens Flare" construction. Completes in a single
* dispatch, no multi-pass ping-pong.
*
* The key point is 【flipping UV before marching toward center】: the output
* pixel's position is point-reflected as uv = 1 - uv, and a vector toward
* center is then stepped from there, so a light source casts its ghosts on
* the OPPOSITE side - matching the arrangement that actually happens in a
* real lens. Without the flip, ghosts would line up on the same side as the
* light source and immediately fly off-screen.
*
* Samples use sampler_linear_wrap, and the weight calculation also uses the
* frac()'d position (matching the weight to the actual post-wrap sample
* position). A clamp sampler would instead stretch the edge pixels for a
* ghost that goes off-screen.
*
* The input is the bright-passed buffer Downsample wrote. This adds onto the
* streak result Compose wrote (a read-modify-write, so a UAV barrier is
* required right before it - see PostProcessRenderer.cpp).
*
* ---------------------------------------------------------------------
*
* [JP]
* Ghost: 画面中心を挟んで反対側へ連なるボケ円のチェーン(ゴースト)と、
* その周りのハロー(輪)。John Chapman の "Pseudo Lens Flare" の構成を
* 踏襲する。1回のディスパッチで完結し、多段階ピンポンは使わない。
*
* 要点は【UVを反転してから中心へ向かって進む】こと: 出力画素の位置を
* uv = 1 - uv と点対称に反転した上で中心へ向かうベクトルを刻むため、光源は
* その反対側にゴーストを落とす - 実際のレンズで起きるのと同じ並び方になる。
* 反転しないと、ゴーストは光源と同じ側へ、しかも画面外へすぐ飛び出す向きに
* 並んでしまう。
*
* サンプルは sampler_linear_wrap で読み、重みの計算にも frac() を掛けた
* 位置を使う(ラップ後の実際のサンプル位置と重みを一致させる)。クランプ
* サンプラだと画面外へ出たゴーストが端の画素を引き伸ばす。
*
* 入力は Downsample が書いたブライトパス済みバッファ。Compose が書いた
* ストリーク結果へ加算する(読み書きなので直前にUAVバリアが必要 -
* PostProcessRenderer.cpp参照)。
*/
[numthreads(8, 8, 1)]
void Ghost(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> output = ResourceDescriptorHeap[constant_indices.post_process_.lens_flare_.unordered_access_view_index_];

	uint width, height;
	output.GetDimensions(width, height);

	/// [EN] Bounds guard: the dispatch is rounded up to a multiple of the
	///      8x8 thread group size, so threads past the actual output edge
	///      must bail out before touching any resource.
	/// [JP] 範囲外ガード: ディスパッチは 8x8 スレッドグループの倍数に
	///      切り上げられているので、実際の出力端を超えたスレッドはどの
	///      リソースにも触れる前に抜ける必要がある。
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.post_process_.lens_flare_streak_.bright_srv_index_];

	float2 uv = 1.0 - (float2(dtid.xy) + 0.5) / float2(width, height);

	/// [EN] The radius calculation is done in aspect-corrected space -
	///      otherwise the halo would come out as a horizontally stretched
	///      ellipse instead of a true circle.
	/// [JP] 半径の計算はアスペクト補正した空間で行う。そうしないとハローが
	///      真円ではなく横長の楕円になる。
	float2 aspect_scale = float2(float(width) / float(height), 1.0);
	float max_radius = length(float2(0.5, 0.5) * aspect_scale);

	uint ghost_count = min(constant_indices.post_process_.lens_flare_.ghost_count_, 8u);
	float ghost_dispersal = constant_indices.post_process_.lens_flare_.ghost_dispersal_;
	float ghost_intensity = constant_indices.post_process_.lens_flare_.ghost_intensity_;
	float halo_width = constant_indices.post_process_.lens_flare_.halo_width_;
	float chromatic_aberration = constant_indices.post_process_.lens_flare_.chromatic_aberration_;
	float intensity = constant_indices.post_process_.lens_flare_.intensity_;

	float3 distortion = float3(-chromatic_aberration, 0.0, chromatic_aberration);

	float2 to_center = float2(0.5, 0.5) - uv;
	float2 ghost_direction = normalize(to_center + 1e-6);
	float2 ghost_vector = to_center * ghost_dispersal;

	float3 result = float3(0.0, 0.0, 0.0);

	[loop]
	for (uint index = 0; index < ghost_count; index++)
	{
		float2 sample_uv = uv + ghost_vector * float(index);
		float radius01 = length((frac(sample_uv) - 0.5) * aspect_scale) / max_radius;
		float weight = pow(saturate(1.0 - radius01), 10.0);

		result += SampleDistorted(source, sample_uv, ghost_direction, distortion) * weight;
	}

	result *= LensColor(saturate(length(to_center * aspect_scale) / max_radius));

	/// [EN] The halo samples shifted a fixed distance toward center - a light
	///      source at radius L shows up as a ring at radius L + halo_width.
	///      The offset is built in aspect space and converted back to UV
	///      space, so it comes out as a true circle on screen.
	/// [JP] ハローは中心へ向かって固定距離だけずらしてサンプルする -
	///      半径 L にある光源が半径 L + halo_width の輪として現れる。
	///      オフセットはアスペクト空間で作ってからUV空間へ戻すことで
	///      画面上で真円になる。
	if (halo_width > 0.0)
	{
		float2 halo_direction = normalize(to_center * aspect_scale + 1e-6) / aspect_scale;
		float2 halo_uv = uv + halo_direction * halo_width;
		float halo_radius01 = length((frac(halo_uv) - 0.5) * aspect_scale) / max_radius;
		float halo_weight = pow(saturate(1.0 - halo_radius01), 5.0);

		result += SampleDistorted(source, halo_uv, halo_direction, distortion) * halo_weight;
	}

	result *= ghost_intensity * intensity;

	output[dtid.xy] = float4(output[dtid.xy].rgb + result, 1.0);
}

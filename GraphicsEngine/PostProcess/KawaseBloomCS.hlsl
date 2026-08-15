#include "../Shader/Constants.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [EN]
* Reference:
* - https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare/
* - https://community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_marius_2D00_slides.pdf
*
* Bloom: the broad glow bright parts of the scene bleed into their
* surroundings. Built as a progressive downsample chain followed by a
* progressive additive upsample chain over 6 levels (level0 = half the
* native resolution, each level half of the one before) - the Kawase-lineage
* structure, using the tap patterns from Jorge Jimenez's "Next Generation
* Post Processing in Call of Duty: Advanced Warfare": a 13-tap downsample
* and a 3x3 tent upsample.
*
* Pass order is DownsamplePrefilter -> Downsample1..5 -> Upsample4..0, so
* level0 ends up holding the final result that ToneMappingCS.hlsl samples
* and adds into the HDR color before exposure. Each pass's source and
* destination level are baked in at HLSL compile time (one entry point per
* pass, the same "one file, many entry points" pattern LensFlareCS.hlsl and
* GlobalIlluminationDenoiseCS.hlsl use) rather than coming from a
* per-dispatch constant buffer - a ConstantBuffer<T> updated more than once
* per frame would race, because every Update() in a frame writes the same
* physical address and the CPU records all passes before the GPU runs any of
* them, so every dispatch would read whichever value was written last.
*
* The Karis average in DownsamplePrefilter is the reason this uses the COD
* tap patterns rather than the cheaper 5-tap/8-tap dual-Kawase filter: with
* ray-traced HDR input a single blown-out subpixel is easy to produce, and
* averaging it naively makes it grow into a large flickering block as it
* propagates up the chain. Weighting each of the four contributing groups by
* 1/(1+luma) before averaging suppresses exactly that.
*
* ---------------------------------------------------------------------
*
* [JP]
* ブルーム: シーンの明るい部分が周囲へにじみ出る広いグロー。6レベル
* (level0 がネイティブ解像度の1/2、以降それぞれ半分ずつ)にわたる段階的な
* ダウンサンプルチェーンと、それに続く段階的な加算アップサンプルチェーン
* として構築する — 構造は Kawase 系列で、タップパターンは Jorge Jimenez の
* 「Next Generation Post Processing in Call of Duty: Advanced Warfare」の
* もの(13タップダウンサンプルと3x3テントアップサンプル)を使う。
*
* パスの順序は DownsamplePrefilter → Downsample1..5 → Upsample4..0 で、
* 最終結果は level0 に入る — ToneMappingCS.hlsl がそれをサンプルして
* 露出適用前のHDRカラーへ加算する。各パスの読み書き先のレベルは
* HLSLのコンパイル時に焼き込む(パスごとに1エントリポイント。
* LensFlareCS.hlsl や GlobalIlluminationDenoiseCS.hlsl と同じ
* 「1ファイル複数エントリポイント」パターン)。ディスパッチごとの
* 定数バッファにしないのは、1フレーム内で複数回 Update() する
* ConstantBuffer<T> が競合するため — フレーム内の Update() は全て同じ
* 物理アドレスへ書き、CPUが全パスを積んでからGPUが動くので、どの
* ディスパッチも「最後に書かれた値」を読んでしまう。
*
* DownsamplePrefilter の Karis 平均が、より安価な5タップ/8タップの
* dual-Kawase ではなくCOD版のタップパターンを採る理由そのもの:
* レイトレHDR入力では1画素だけ飛び抜けて明るい点が簡単に発生し、
* それを素直に平均するとチェーンを昇るにつれて巨大なちらつくブロックに
* 育ってしまう。寄与する4グループそれぞれを平均前に 1/(1+luma) で
* 重み付けすると、それがちょうど抑えられる。
*/

uint BloomUnorderedAccessViewIndex(uint level)
{
	if (level == 0)
	{
		return constant_indices.post_process_.bloom_.level0_uav_index_;
	}
	if (level == 1)
	{
		return constant_indices.post_process_.bloom_.level1_uav_index_;
	}
	if (level == 2)
	{
		return constant_indices.post_process_.bloom_.level2_uav_index_;
	}
	if (level == 3)
	{
		return constant_indices.post_process_.bloom_.level3_uav_index_;
	}
	if (level == 4)
	{
		return constant_indices.post_process_.bloom_.level4_uav_index_;
	}
	return constant_indices.post_process_.bloom_.level5_uav_index_;
}

uint BloomShaderResourceViewIndex(uint level)
{
	if (level == 0)
	{
		return constant_indices.post_process_.bloom_.level0_srv_index_;
	}
	if (level == 1)
	{
		return constant_indices.post_process_.bloom_.level1_srv_index_;
	}
	if (level == 2)
	{
		return constant_indices.post_process_.bloom_.level2_srv_index_;
	}
	if (level == 3)
	{
		return constant_indices.post_process_.bloom_.level3_srv_index_;
	}
	if (level == 4)
	{
		return constant_indices.post_process_.bloom_.level4_srv_index_;
	}
	return constant_indices.post_process_.bloom_.level5_srv_index_;
}

float Luminance(float3 color)
{
	return dot(color, float3(0.2126, 0.7152, 0.0722));
}

// [JP] Karis平均の重み。明るい画素ほど小さい重みになるので、1画素だけ
//      極端に明るい点(ファイアフライ)が平均結果を支配しなくなる。
float KarisWeight(float3 color)
{
	return 1.0 / (1.0 + Luminance(color));
}

// [JP] シーンカラーを取り込む前の検査。ブルームはチェーンの最初にシーンカラーへ
//      触る効果なので、ここが非有限値の入口になる。しかも Karis 重みは
//      「明るいほど小さい重み」なので、色が +Inf のとき重みは 0 になり、
//      color * weight が Inf * 0 = NaN を生む — ファイアフライ抑制の仕組み
//      そのものが NaN の発生源になるという、直感に反する経路。
//
//      生まれた NaN はダウンサンプルチェーンを上りながら広がり、同じ明部
//      バッファを読むレンズフレアやアナモルフィックフレアへも渡るため、
//      1点の破綻が画面上の複数の無関係な場所へ、各効果のカーネル形状で
//      黒として現れる。値を作った側(ライティング)の対策とは別に、
//      取り込み口でも必ず畳んでおく。
float3 SanitizeSceneColor(float3 color)
{
	bool invalid = any(isnan(color)) || any(isinf(color));
	return invalid ? float3(0, 0, 0) : max(color, 0.0);
}

// [JP] ソフトニー付きのしきい値(UE4式)。threshold_ で硬く切ると、
//      しきい値付近を行き来する画素がフレームごとに出たり消えたりして
//      ちらつくため、soft_knee_ の幅だけ二次曲線で滑らかに立ち上げる。
float3 SoftThreshold(float3 color, float threshold, float soft_knee)
{
	float knee = max(threshold * soft_knee, 1e-4);
	float brightest = max(color.r, max(color.g, color.b));

	float soft = clamp(brightest - threshold + knee, 0.0, 2.0 * knee);
	soft = soft * soft / (4.0 * knee);

	float contribution = max(soft, brightest - threshold) / max(brightest, 1e-4);
	return color * contribution;
}

// [JP] COD:AWの13タップダウンサンプル。中心と内側4点に重みを寄せた
//      形で、合計がちょうど1.0になる(0.125*5 + 0.0625*4 + 0.03125*4)。
//      texel は【ソース側】のテクセルサイズ。
float3 Downsample13Tap(Texture2D<float4> source, float2 uv, float2 texel)
{
	float3 a = source.SampleLevel(sampler_linear_clamp, uv + float2(-2.0, 2.0) * texel, 0).rgb;
	float3 b = source.SampleLevel(sampler_linear_clamp, uv + float2(0.0, 2.0) * texel, 0).rgb;
	float3 c = source.SampleLevel(sampler_linear_clamp, uv + float2(2.0, 2.0) * texel, 0).rgb;

	float3 d = source.SampleLevel(sampler_linear_clamp, uv + float2(-2.0, 0.0) * texel, 0).rgb;
	float3 e = source.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
	float3 f = source.SampleLevel(sampler_linear_clamp, uv + float2(2.0, 0.0) * texel, 0).rgb;

	float3 g = source.SampleLevel(sampler_linear_clamp, uv + float2(-2.0, -2.0) * texel, 0).rgb;
	float3 h = source.SampleLevel(sampler_linear_clamp, uv + float2(0.0, -2.0) * texel, 0).rgb;
	float3 i = source.SampleLevel(sampler_linear_clamp, uv + float2(2.0, -2.0) * texel, 0).rgb;

	float3 j = source.SampleLevel(sampler_linear_clamp, uv + float2(-1.0, 1.0) * texel, 0).rgb;
	float3 k = source.SampleLevel(sampler_linear_clamp, uv + float2(1.0, 1.0) * texel, 0).rgb;
	float3 l = source.SampleLevel(sampler_linear_clamp, uv + float2(-1.0, -1.0) * texel, 0).rgb;
	float3 m = source.SampleLevel(sampler_linear_clamp, uv + float2(1.0, -1.0) * texel, 0).rgb;

	float3 result = e * 0.125;
	result += (a + c + g + i) * 0.03125;
	result += (b + d + f + h) * 0.0625;
	result += (j + k + l + m) * 0.125;
	return result;
}

// [JP] Downsample13Tap と同じ13タップだが、内側の4点グループごとに
//      Karis重みを掛けてから平均する。初段(フル解像度のシーンカラーを
//      読む所)だけで使う — ファイアフライが入ってくるのはそこだけで、
//      以降のレベルは既に平均済みだから。
float3 Downsample13TapKaris(Texture2D<float4> source, float2 uv, float2 texel)
{
	float3 a = SanitizeSceneColor(source.SampleLevel(sampler_linear_clamp, uv + float2(-2.0, 2.0) * texel, 0).rgb);
	float3 b = SanitizeSceneColor(source.SampleLevel(sampler_linear_clamp, uv + float2(0.0, 2.0) * texel, 0).rgb);
	float3 c = SanitizeSceneColor(source.SampleLevel(sampler_linear_clamp, uv + float2(2.0, 2.0) * texel, 0).rgb);

	float3 d = SanitizeSceneColor(source.SampleLevel(sampler_linear_clamp, uv + float2(-2.0, 0.0) * texel, 0).rgb);
	float3 e = SanitizeSceneColor(source.SampleLevel(sampler_linear_clamp, uv, 0).rgb);
	float3 f = SanitizeSceneColor(source.SampleLevel(sampler_linear_clamp, uv + float2(2.0, 0.0) * texel, 0).rgb);

	float3 g = SanitizeSceneColor(source.SampleLevel(sampler_linear_clamp, uv + float2(-2.0, -2.0) * texel, 0).rgb);
	float3 h = SanitizeSceneColor(source.SampleLevel(sampler_linear_clamp, uv + float2(0.0, -2.0) * texel, 0).rgb);
	float3 i = SanitizeSceneColor(source.SampleLevel(sampler_linear_clamp, uv + float2(2.0, -2.0) * texel, 0).rgb);

	float3 j = SanitizeSceneColor(source.SampleLevel(sampler_linear_clamp, uv + float2(-1.0, 1.0) * texel, 0).rgb);
	float3 k = SanitizeSceneColor(source.SampleLevel(sampler_linear_clamp, uv + float2(1.0, 1.0) * texel, 0).rgb);
	float3 l = SanitizeSceneColor(source.SampleLevel(sampler_linear_clamp, uv + float2(-1.0, -1.0) * texel, 0).rgb);
	float3 m = SanitizeSceneColor(source.SampleLevel(sampler_linear_clamp, uv + float2(1.0, -1.0) * texel, 0).rgb);

	/// [JP] 13タップを重なり合う5つの2x2グループに分け、グループごとに
	///      Karis重みで加重平均する。COD:AW の講演どおりの分け方。
	float3 group0 = (a + b + d + e) * 0.25;
	float3 group1 = (b + c + e + f) * 0.25;
	float3 group2 = (d + e + g + h) * 0.25;
	float3 group3 = (e + f + h + i) * 0.25;
	float3 group4 = (j + k + l + m) * 0.25;

	float weight0 = KarisWeight(group0) * 0.125;
	float weight1 = KarisWeight(group1) * 0.125;
	float weight2 = KarisWeight(group2) * 0.125;
	float weight3 = KarisWeight(group3) * 0.125;
	float weight4 = KarisWeight(group4) * 0.5;

	float weight_sum = weight0 + weight1 + weight2 + weight3 + weight4;
	float3 result = group0 * weight0 + group1 * weight1 + group2 * weight2 + group3 * weight3 + group4 * weight4;
	return result / max(weight_sum, 1e-4);
}

// [JP] COD:AWの3x3テントアップサンプル(中心4、辺2、角1、合計16)。
//      radius はUV単位で、レベルによらず同じ値を使う — 低解像度レベル
//      ほど1テクセルが大きいので、同じUV半径でも自然に広い滲みになる。
float3 UpsampleTent(Texture2D<float4> source, float2 uv, float radius)
{
	float3 a = source.SampleLevel(sampler_linear_clamp, uv + float2(-radius, radius), 0).rgb;
	float3 b = source.SampleLevel(sampler_linear_clamp, uv + float2(0.0, radius), 0).rgb;
	float3 c = source.SampleLevel(sampler_linear_clamp, uv + float2(radius, radius), 0).rgb;

	float3 d = source.SampleLevel(sampler_linear_clamp, uv + float2(-radius, 0.0), 0).rgb;
	float3 e = source.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
	float3 f = source.SampleLevel(sampler_linear_clamp, uv + float2(radius, 0.0), 0).rgb;

	float3 g = source.SampleLevel(sampler_linear_clamp, uv + float2(-radius, -radius), 0).rgb;
	float3 h = source.SampleLevel(sampler_linear_clamp, uv + float2(0.0, -radius), 0).rgb;
	float3 i = source.SampleLevel(sampler_linear_clamp, uv + float2(radius, -radius), 0).rgb;

	float3 result = e * 4.0;
	result += (b + d + f + h) * 2.0;
	result += (a + c + g + i);
	return result * (1.0 / 16.0);
}

// [JP] Downsample1..5 の共通本体。読み書きするレベルは呼び出し側の
//      エントリポイントがコンパイル時定数として渡す。
void DownsampleLevel(uint3 dtid, uint source_level, uint destination_level)
{
	RWTexture2D<float4> destination = ResourceDescriptorHeap[BloomUnorderedAccessViewIndex(destination_level)];

	uint width, height;
	destination.GetDimensions(width, height);
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[BloomShaderResourceViewIndex(source_level)];

	uint source_width, source_height;
	source.GetDimensions(source_width, source_height);
	float2 source_texel = 1.0 / float2(source_width, source_height);

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);

	destination[dtid.xy] = float4(Downsample13Tap(source, uv, source_texel), 1.0);
}

// [JP] Upsample4..0 の共通本体。1つ下(低解像度)のレベルをテントで
//      引き伸ばし、書き込み先レベルへ【加算】する(read-modify-write)。
//      加算なのでチェーンを上がるにつれて全レベルの寄与が積み上がり、
//      広い滲みと細かい滲みが同時に乗る。
void UpsampleLevel(uint3 dtid, uint source_level, uint destination_level)
{
	RWTexture2D<float4> destination = ResourceDescriptorHeap[BloomUnorderedAccessViewIndex(destination_level)];

	uint width, height;
	destination.GetDimensions(width, height);
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[BloomShaderResourceViewIndex(source_level)];

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);
	float radius = constant_indices.post_process_.bloom_.filter_radius_;

	float3 result = destination[dtid.xy].rgb + UpsampleTent(source, uv, radius);
	destination[dtid.xy] = float4(result, 1.0);
}

[numthreads(8, 8, 1)]
void DownsamplePrefilter(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> destination = ResourceDescriptorHeap[constant_indices.post_process_.bloom_.level0_uav_index_];

	uint width, height;
	destination.GetDimensions(width, height);
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
	float threshold = constant_indices.post_process_.bloom_.threshold_;
	float soft_knee = constant_indices.post_process_.bloom_.soft_knee_;

	float3 result = Downsample13TapKaris(source, uv, source_texel);
	result = SoftThreshold(result, threshold, soft_knee);

	destination[dtid.xy] = float4(result, 1.0);
}

[numthreads(8, 8, 1)]
void Downsample1(uint3 dtid : SV_DispatchThreadID)
{
	DownsampleLevel(dtid, 0, 1);
}

[numthreads(8, 8, 1)]
void Downsample2(uint3 dtid : SV_DispatchThreadID)
{
	DownsampleLevel(dtid, 1, 2);
}

[numthreads(8, 8, 1)]
void Downsample3(uint3 dtid : SV_DispatchThreadID)
{
	DownsampleLevel(dtid, 2, 3);
}

[numthreads(8, 8, 1)]
void Downsample4(uint3 dtid : SV_DispatchThreadID)
{
	DownsampleLevel(dtid, 3, 4);
}

[numthreads(8, 8, 1)]
void Downsample5(uint3 dtid : SV_DispatchThreadID)
{
	DownsampleLevel(dtid, 4, 5);
}

[numthreads(8, 8, 1)]
void Upsample4(uint3 dtid : SV_DispatchThreadID)
{
	UpsampleLevel(dtid, 5, 4);
}

[numthreads(8, 8, 1)]
void Upsample3(uint3 dtid : SV_DispatchThreadID)
{
	UpsampleLevel(dtid, 4, 3);
}

[numthreads(8, 8, 1)]
void Upsample2(uint3 dtid : SV_DispatchThreadID)
{
	UpsampleLevel(dtid, 3, 2);
}

[numthreads(8, 8, 1)]
void Upsample1(uint3 dtid : SV_DispatchThreadID)
{
	UpsampleLevel(dtid, 2, 1);
}

[numthreads(8, 8, 1)]
void Upsample0(uint3 dtid : SV_DispatchThreadID)
{
	UpsampleLevel(dtid, 1, 0);
}

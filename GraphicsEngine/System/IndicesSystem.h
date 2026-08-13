#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>

namespace SeedCore
{
	/// [EN] Upper bound on LensFlareCS.hlsl's independently-blurred spike
	///      axes (one axis = one line = two opposing arms). Diffraction
	///      through an n-bladed iris yields n spikes when n is even and 2n
	///      when n is odd, because each blade edge diffracts perpendicular
	///      to itself and on an even-bladed iris the opposing edges are
	///      parallel so their spikes coincide. That makes the axis count
	///      n/2 for even n and n for odd n, and BokehSettings::bladeCount_
	///      (the same physical iris) is clamped to 3..8, so the worst case
	///      is n = 7 -> 14 spikes -> 7 axes. Must match
	///      LENS_FLARE_MAX_AXIS_COUNT in Shader/Constants.hlsli.
	/// [JP] LensFlareCS.hlsl が独立にブラーする棘の軸数の上限(軸1本 =
	///      直線1本 = 対向する腕2本)。羽根n枚の絞りによる回折は、nが偶数
	///      ならn本、奇数なら2n本の棘を作る — 各羽根のエッジがそれ自身に
	///      垂直な方向へ回折し、偶数枚だと向かい合うエッジが平行になって
	///      棘が重なるため。よって軸数は偶数なら n/2、奇数なら n。
	///      BokehSettings::bladeCount_(同じ物理的な絞り)が3〜8に
	///      クランプされているので、最悪ケースは n = 7 → 棘14本 → 7軸。
	///      Shader/Constants.hlsli の LENS_FLARE_MAX_AXIS_COUNT と
	///      一致していなければならない。
	inline constexpr Uint32 lensFlareMaxAxisCount = 7;

	/// [EN] Per-view auto-exposure indices/tuning — same one-effect-one-group
	///      shape as ShadowAccumulationIndices etc. below.
	/// [JP] ビューごとの自動露出インデックス/チューニング。下の
	///      ShadowAccumulationIndices 等と同じ「エフェクトごとに1グループ」の形。
	struct ExposureIndices
	{
		Uint histogramUnorderedAccessViewIndex_ = 0;
		Uint exposureUnorderedAccessViewIndex_ = 0;
		Uint autoExposureEnabled_ = 0;
		Float exposureCompensation_ = 0.0f;

		Float minLogLuminance_ = 0.0f;
		Float maxLogLuminance_ = 0.0f;
		Float keyValue_ = 0.0f;
		Float adaptSpeedToBright_ = 0.0f;

		Float adaptSpeedToDark_ = 0.0f;
		Uint exposurePadding_[3] = { 0, 0, 0 };
	};
	static_assert(sizeof(ExposureIndices) % 16 == 0, "ExposureIndices が 16 バイト行の倍数ではありません");

	/// [EN] Per-view tone-mapping indices/tuning.
	/// [JP] ビューごとのトーンマップインデックス/チューニング。
	struct ToneMappingIndices
	{
		Uint toneMappingEnabled_ = 0;
		Uint toneMappingMode_ = 0;
		Uint toneMappingPadding_[2] = { 0, 0 };
	};
	static_assert(sizeof(ToneMappingIndices) % 16 == 0, "ToneMappingIndices が 16 バイト行の倍数ではありません");

	/// [EN] Per-view lens-flare indices/tuning. unorderedAccessViewIndex_/
	///      shaderResourceViewIndex_ are LensFlareCS.hlsl's quarter-res write
	///      target and the bindless SRV ToneMappingCS.hlsl samples to add the
	///      flare into the HDR color before exposure. enabled_ gates both:
	///      when off, PostProcessRenderer's Dispatch skips LensFlareCS
	///      entirely, so ToneMappingCS must also skip the read to avoid
	///      sampling stale leftover data from when it was last enabled.
	/// [JP] ビューごとのレンズフレアインデックス/チューニング。
	///      unorderedAccessViewIndex_/shaderResourceViewIndex_ は
	///      LensFlareCS.hlsl の書き込み先(1/4解像度)と、ToneMappingCS.hlsl が
	///      露出適用前のHDRカラーへフレアを加算するためにサンプルする
	///      bindless SRV。enabled_ は両方を制御する: 無効時は
	///      PostProcessRenderer 側の Dispatch が LensFlareCS を丸ごと
	///      スキップするので、ToneMappingCS 側も読み取りをスキップしないと、
	///      有効だった時の古いデータをサンプルしてしまう。
	struct LensFlareIndices
	{
		Uint enabled_ = 0;
		Uint unorderedAccessViewIndex_ = 0;
		Uint shaderResourceViewIndex_ = 0;
		Float threshold_ = 0.0f;

		Float intensity_ = 0.0f;
		Float streakLength_ = 0.0f;
		Float streakAttenuation_ = 0.0f;
		Float chromaticAberration_ = 0.0f;

		Float angleOffset_ = 0.0f;
		Uint ghostCount_ = 4;
		Float ghostDispersal_ = 0.3f;
		Float ghostIntensity_ = 0.3f;

		Float haloWidth_ = 0.45f;
		Uint axisCount_ = 3;
		Float spikeVariation_ = 0.0f;
		Uint lensFlarePadding_ = 0;
	};
	static_assert(sizeof(LensFlareIndices) % 16 == 0, "LensFlareIndices が 16 バイト行の倍数ではありません");

	/// [EN] Per-view lens-flare working buffers: one ping/pong pair per axis
	///      (LensFlareCS.hlsl walks LensFlareIndices::axisCount_ axes, each a
	///      line of two opposing arms, each independently multi-pass blurred
	///      - the count comes from the aperture blade count, see
	///      lensFlareMaxAxisCount above), plus the shared
	///      bright_ buffer every other lens-flare pass reads. bright_ is what
	///      LensFlareCS.hlsl's Downsample entry point writes: a quarter-res,
	///      bright-passed copy of the scene produced with a 4-tap bilinear
	///      filter covering the full 4x4 source footprint. Both BlurPass1
	///      (streaks) and Ghost (ghost chain + halo) sample it instead of the
	///      full-res HDR source, because point-sampling mip 0 at quarter
	///      density skips 15 of every 16 source pixels and so drops small
	///      bright sources entirely - the sun disc (a few pixels wide, see
	///      VolumetricCloudScapes.hlsli::ProceduralSkyColor) would otherwise
	///      never produce a flare. Set once per frame alongside
	///      LensFlareIndices above (PostProcessRenderer::CreateView allocates
	///      them, PrepareView registers the bindless indices) - not
	///      re-registered mid-frame, since LensFlareCS.hlsl's BlurPass1..4
	///      entry points read/write them by a compile-time-fixed ping/pong
	///      parity per pass, not a per-dispatch index.
	/// [JP] ビューごとのレンズフレア作業バッファ。軸ごとに1ペアのピンポン
	///      バッファ(LensFlareCS.hlsl は LensFlareIndices::axisCount_ 軸 -
	///      それぞれ対向する腕2本からなる直線 - を独立に多段階ブラーする。
	///      軸数は絞りの羽根枚数から決まる、上の lensFlareMaxAxisCount 参照)
	///      と、他の全レンズフレアパスが読む
	///      共有の bright_ バッファ。bright_ は LensFlareCS.hlsl の Downsample
	///      エントリポイントが書く、1/4解像度のブライトパス済みシーンコピーで、
	///      4x4のソース範囲を丸ごとカバーする4タップのバイリニアフィルタで
	///      生成する。BlurPass1(ストリーク)も Ghost(ゴーストチェーン+ハロー)も
	///      フル解像度のHDRソースではなくこちらをサンプルする — 1/4密度で
	///      mip 0 をポイントサンプルすると16画素中15画素を読み飛ばすため、
	///      小さく明るい光源が丸ごと消えてしまうから。太陽の円盤(数画素幅、
	///      VolumetricCloudScapes.hlsli::ProceduralSkyColor 参照)は
	///      そうしないと一切フレアを出さない。上のLensFlareIndicesと同時に
	///      フレームに1回だけ設定する(PostProcessRenderer::CreateView が
	///      確保し、PrepareView が bindless インデックスを登録する) -
	///      フレーム中に再登録はしない。LensFlareCS.hlsl の BlurPass1..4
	///      エントリポイントはパスごとにコンパイル時固定のピンポン奇偶で
	///      読み書きするため、ディスパッチごとのインデックスは不要。
	struct LensFlareStreakIndices
	{
		/// [EN] One 16-byte row per axis: [0] ping UAV, [1] ping SRV,
		///      [2] pong UAV, [3] pong SRV. Mirrors a uint4 array on the
		///      HLSL side rather than flat fields, because a uint4 array
		///      element is exactly one cbuffer row and so is directly
		///      indexable by a runtime axis number - flat fields would need
		///      a 7-way if-chain per lookup. All lensFlareMaxAxisCount rows
		///      are always filled; only the first axisCount_ are read.
		/// [JP] 軸ごとに16バイト1行: [0] ping UAV、[1] ping SRV、
		///      [2] pong UAV、[3] pong SRV。HLSL側は uint4 配列で、フラットな
		///      フィールドの並びにしていないのは、uint4 配列の要素が
		///      ちょうどcbufferの1行なので実行時の軸番号でそのまま添字
		///      アクセスできるため — フラットだと参照のたびに7分岐の
		///      if連鎖が要る。lensFlareMaxAxisCount 行ぶん常に埋めるが、
		///      読まれるのは先頭 axisCount_ 行だけ。
		Uint axisIndices_[lensFlareMaxAxisCount][4] = {};

		Uint brightUnorderedAccessViewIndex_ = 0;
		Uint brightShaderResourceViewIndex_ = 0;
		Uint lensFlareStreakPadding_[2] = { 0, 0 };
	};
	static_assert(sizeof(LensFlareStreakIndices) % 16 == 0, "LensFlareStreakIndices が 16 バイト行の倍数ではありません");

	/// [EN] Per-view bloom indices/tuning. level0..5 are the 6 levels of
	///      KawaseBloomCS.hlsl's downsample/upsample chain, level0 being
	///      half the native resolution and each subsequent level half of the
	///      one before. The chain writes DOWN through the levels
	///      (DownsamplePrefilter then Downsample1..5) then accumulates back
	///      UP additively (Upsample4..0), so level0 ends up holding the final
	///      bloom that ToneMappingCS.hlsl samples and adds into the HDR color
	///      before exposure. enabled_ gates both the dispatch and that read,
	///      so a stale buffer from when bloom was last on is never sampled.
	///      filterRadius_ is the 3x3 tent radius in UV used by the upsample
	///      passes; softKnee_ widens the threshold_ transition so pixels
	///      sitting at the cutoff fade in instead of popping.
	/// [JP] ビューごとのブルームインデックス/チューニング。level0..5 は
	///      KawaseBloomCS.hlsl のダウンサンプル/アップサンプルチェーンの
	///      6レベルで、level0 がネイティブ解像度の1/2、以降それぞれ半分ずつ。
	///      チェーンはレベルを【下って】書き
	///      (DownsamplePrefilter → Downsample1..5)、
	///      そこから【上って】加算で積み上げる(Upsample4..0)ため、最終的な
	///      ブルームは level0 に入る — ToneMappingCS.hlsl がそれをサンプル
	///      して露出適用前のHDRカラーへ加算する。enabled_ はディスパッチと
	///      その読み取りの両方を制御するので、前回有効だった時の古い
	///      バッファをサンプルすることはない。filterRadius_ はアップサンプル
	///      パスが使う3x3テントの半径(UV単位)、softKnee_ は threshold_
	///      周辺の遷移を広げ、しきい値ぎりぎりの画素が突然出入りせず
	///      滑らかにフェードするようにする。
	struct BloomIndices
	{
		Uint level0UnorderedAccessViewIndex_ = 0;
		Uint level1UnorderedAccessViewIndex_ = 0;
		Uint level2UnorderedAccessViewIndex_ = 0;
		Uint level3UnorderedAccessViewIndex_ = 0;

		Uint level4UnorderedAccessViewIndex_ = 0;
		Uint level5UnorderedAccessViewIndex_ = 0;
		Uint level0ShaderResourceViewIndex_ = 0;
		Uint level1ShaderResourceViewIndex_ = 0;

		Uint level2ShaderResourceViewIndex_ = 0;
		Uint level3ShaderResourceViewIndex_ = 0;
		Uint level4ShaderResourceViewIndex_ = 0;
		Uint level5ShaderResourceViewIndex_ = 0;

		Uint enabled_ = 0;
		Float threshold_ = 0.0f;
		Float softKnee_ = 0.0f;
		Float intensity_ = 0.0f;

		Float filterRadius_ = 0.0f;
		Uint bloomPadding_[3] = { 0, 0, 0 };
	};
	static_assert(sizeof(BloomIndices) % 16 == 0, "BloomIndices が 16 バイト行の倍数ではありません");

	/// [EN] Per-view anamorphic-flare indices/tuning. ping_/pong_ are
	///      AnamorphicFlareCS.hlsl's HORIZONTALLY SQUEEZED working buffers -
	///      half the width of the other quarter-res post-process buffers, so
	///      they carry a baked 2:1 anamorphic squeeze. That squeeze is why
	///      the streak comes out horizontal at all: the flare is blurred as
	///      an ordinary round shape inside the squeezed space and Compose
	///      samples it back with normal UVs, which stretches it 2x
	///      horizontally for free. output_ is Compose's own target, which
	///      ToneMappingCS.hlsl samples and adds into the HDR color before
	///      exposure. enabled_ gates both the dispatch and that read, so a
	///      stale buffer from when the effect was last on is never sampled.
	/// [JP] ビューごとのアナモルフィックフレアインデックス/チューニング。
	///      ping_/pong_ は AnamorphicFlareCS.hlsl の【横に圧縮された】作業
	///      バッファで、他の1/4解像度ポストプロセスバッファの半分の幅
	///      = 2:1 のアナモルフィック圧縮を焼き込んである。そもそも筋が
	///      横向きになるのはこの圧縮のおかげ: 圧縮空間の中では普通の丸い
	///      形としてブラーし、Compose が通常のUVでサンプルして戻すことで、
	///      横方向に2倍引き伸ばされる。output_ は Compose 自身の書き込み先で、
	///      ToneMappingCS.hlsl がこれをサンプルして露出適用前のHDRカラーへ
	///      加算する。enabled_ はディスパッチとその読み取りの両方を制御
	///      するので、前回有効だった時の古いバッファを読むことはない。
	struct AnamorphicFlareIndices
	{
		Uint enabled_ = 0;
		Uint outputUnorderedAccessViewIndex_ = 0;
		Uint outputShaderResourceViewIndex_ = 0;
		Float threshold_ = 0.0f;

		Uint pingUnorderedAccessViewIndex_ = 0;
		Uint pingShaderResourceViewIndex_ = 0;
		Uint pongUnorderedAccessViewIndex_ = 0;
		Uint pongShaderResourceViewIndex_ = 0;

		Float intensity_ = 0.0f;
		Float streakLength_ = 0.0f;
		Float attenuation_ = 0.0f;
		Uint anamorphicFlarePadding_ = 0;

		Float tint_[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	};
	static_assert(sizeof(AnamorphicFlareIndices) % 16 == 0, "AnamorphicFlareIndices が 16 バイト行の倍数ではありません");

	/// [EN] Per-view chromatic-aberration and vignette indices/tuning. Both
	///      are LENS stage effects: they run before auto-exposure and tone
	///      mapping, because both describe what reaches the sensor rather
	///      than how the sensor is developed. They chain through one shared
	///      buffer - sourceShaderResourceViewIndex_ is resolved on the CPU in
	///      PostProcessRenderer::PrepareView, so if chromatic aberration is
	///      on the vignette reads its output, otherwise it reads the
	///      depth-of-field output or the raw scene color. Vignette is a pure
	///      per-pixel multiply and so may read and write the same texture in
	///      place; chromatic aberration reads neighbours and may not, which
	///      is why it always writes the shared lens-stage buffer.
	/// [JP] ビューごとの色収差/ビネットのインデックス・チューニング。
	///      どちらも【レンズ段】のエフェクトで、自動露出とトーンマップより
	///      前に走る — どちらも「センサーへ何が届くか」を記述するもので
	///      あって、「センサーをどう現像するか」ではないため。両者は1枚の
	///      共有バッファで連鎖する: sourceShaderResourceViewIndex_ は
	///      PostProcessRenderer::PrepareView がCPU側で解決するので、色収差が
	///      有効ならビネットはその出力を、無効なら被写界深度の出力または
	///      生のシーンカラーを読む。ビネットは画素ごとの単純な乗算なので
	///      同じテクスチャを読み書きしてよいが、色収差は近傍を読むため
	///      それができない — だから常に共有のレンズ段バッファへ書く。
	/// [EN] One tonal range's colour grading controls, in the order they are
	///      applied. All scalars rather than per-channel: the colour axis is
	///      handled by temperature_ as a chromatic adaptation instead (see
	///      ColorGradingRangeSettings in PostProcess.h for why). Neutral is 1
	///      for saturation/contrast/gamma/gain and 0 for offset/temperature.
	/// [JP] 1つの階調域ぶんのカラーグレーディング操作、適用される順。
	///      チャンネル別ではなく全てスカラー — 色方向は temperature_ が
	///      色順応として担当する(理由は PostProcess.h の
	///      ColorGradingRangeSettings 参照)。中立値は彩度/コントラスト/
	///      ガンマ/ゲインが1、オフセットと色温度が0。
	struct ColorGradingRangeIndices
	{
		Float temperature_ = 0.0f;
		Float saturation_ = 1.0f;
		Float contrast_ = 1.0f;
		Float gamma_ = 1.0f;

		Float gain_ = 1.0f;
		Float offset_ = 0.0f;
		Uint colorGradingRangePadding_[2] = { 0, 0 };
	};
	static_assert(sizeof(ColorGradingRangeIndices) % 16 == 0, "ColorGradingRangeIndices が 16 バイト行の倍数ではありません");

	/// [EN] Unreal-style colour grading: four tonal ranges each with their
	///      own wheels, blended by luminance with smooth crossovers at
	///      shadowsMax_ and highlightsMin_. Runs in scene-referred linear
	///      space AFTER exposure and BEFORE the tone curve, which is forced
	///      by the 0.18 contrast pivot - 0.18 only means middle grey once
	///      exposure has placed the scene there, and means nothing after the
	///      curve has compressed the range. Because of that position this
	///      pass also owns the additive contributions (bloom, lens flare,
	///      anamorphic) and the exposure multiply, which ToneMappingCS.hlsl
	///      skips whenever enabled_ is set.
	/// [JP] Unreal 方式のカラーグレーディング: 4つの階調域がそれぞれ
	///      ホイールを持ち、輝度によって shadowsMax_ と highlightsMin_ の
	///      なだらかなクロスオーバーでブレンドされる。露出の【後】、
	///      トーンカーブの【前】、シーン参照リニア空間で走る。この位置は
	///      0.18 のコントラスト軸が強制するもので、0.18 が中間グレーを
	///      意味するのは露出がシーンをそこへ置いた後だけ、カーブがレンジを
	///      圧縮した後では何の意味も持たない。この位置ゆえに、このパスは
	///      加算寄与(ブルーム、レンズフレア、アナモルフィック)と露出の
	///      乗算も担当し、enabled_ が立っている間 ToneMappingCS.hlsl は
	///      それらをスキップする。
	struct ColorGradingIndices
	{
		Uint enabled_ = 0;
		Uint sourceShaderResourceViewIndex_ = 0;
		Uint destinationUnorderedAccessViewIndex_ = 0;
		Float shadowsMax_ = 0.0f;

		Float highlightsMin_ = 0.0f;
		Uint outputShaderResourceViewIndex_ = 0;
		Uint colorGradingPadding_[2] = { 0, 0 };

		ColorGradingRangeIndices global_;
		ColorGradingRangeIndices shadows_;
		ColorGradingRangeIndices midtones_;
		ColorGradingRangeIndices highlights_;
	};
	static_assert(sizeof(ColorGradingIndices) % 16 == 0, "ColorGradingIndices が 16 バイト行の倍数ではありません");

	/// [EN] Radial half of the Brown-Conrady distortion model, the first
	///      stage of the lens chain since it displaces geometry. k1_
	///      dominates, k2_ refines the corners, k3_ barely moves anything.
	///      scale_ zooms in before distorting so barrel distortion does not
	///      leave empty corners.
	/// [JP] Brown-Conrady 歪曲モデルの半径方向の項。ジオメトリを変位させる
	///      ので、レンズ連鎖の最初の段になる。k1_ が支配的、k2_ が四隅の
	///      微調整、k3_ はほとんど動かない。scale_ は歪ませる前の拡大で、
	///      樽型歪曲が四隅に余白を作らないようにするためのもの。
	struct LensDistortionIndices
	{
		Uint enabled_ = 0;
		Uint sourceShaderResourceViewIndex_ = 0;
		Uint destinationUnorderedAccessViewIndex_ = 0;
		Float k1_ = 0.0f;

		Float k2_ = 0.0f;
		Float k3_ = 0.0f;
		Float scale_ = 1.0f;
		Uint lensDistortionPadding_ = 0;
	};
	static_assert(sizeof(LensDistortionIndices) % 16 == 0, "LensDistortionIndices が 16 バイト行の倍数ではありません");

	/// [EN] Film grain. Runs LAST, after SharpnessCS.hlsl, and
	///      read-modify-writes that pass's output in place - safe because
	///      grain is a per-pixel operation with no neighbour taps, and
	///      deliberate so the sharpen pass does not amplify the grain it was
	///      given. Unlike the lens-stage effects this is applied after tone
	///      mapping: grain is the developed emulsion's density variation, so
	///      the tonal position driving luminanceResponse_ only means anything
	///      post-curve.
	/// [JP] フィルムグレイン。SharpnessCS.hlsl の後、最後に走り、その出力を
	///      その場で read-modify-write する — グレインは近傍タップの無い
	///      画素ごとの処理なので安全であり、かつシャープパスがグレインを
	///      増幅しないようにするための意図的な順序。レンズ段のエフェクトと
	///      違いトーンマップの後に適用する: グレインは現像された乳剤の
	///      濃度ムラなので、luminanceResponse_ を駆動する階調上の位置は
	///      カーブを通した後でなければ意味を持たない。
	struct FilmGrainIndices
	{
		Uint enabled_ = 0;
		Uint destinationUnorderedAccessViewIndex_ = 0;
		Uint colored_ = 0;
		Float intensity_ = 0.0f;

		Float size_ = 0.0f;
		Float luminanceResponse_ = 0.0f;
		Uint filmGrainPadding_[2] = { 0, 0 };
	};
	static_assert(sizeof(FilmGrainIndices) % 16 == 0, "FilmGrainIndices が 16 バイト行の倍数ではありません");

	struct ChromaticAberrationIndices
	{
		Uint enabled_ = 0;
		Uint sourceShaderResourceViewIndex_ = 0;
		Uint destinationUnorderedAccessViewIndex_ = 0;
		Float intensity_ = 0.0f;

		Uint sampleCount_ = 8;
		Uint chromaticAberrationPadding_[3] = { 0, 0, 0 };
	};
	static_assert(sizeof(ChromaticAberrationIndices) % 16 == 0, "ChromaticAberrationIndices が 16 バイト行の倍数ではありません");

	struct VignetteIndices
	{
		Uint enabled_ = 0;
		Uint sourceShaderResourceViewIndex_ = 0;
		Uint destinationUnorderedAccessViewIndex_ = 0;
		Float intensity_ = 0.0f;

		Float exponent_ = 4.0f;
		Uint vignettePadding_[3] = { 0, 0, 0 };

		Float color_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	};
	static_assert(sizeof(VignetteIndices) % 16 == 0, "VignetteIndices が 16 バイト行の倍数ではありません");

	/// [EN] Per-view depth-of-field indices/tuning. unorderedAccessViewIndex_/
	///      shaderResourceViewIndex_ are DepthOfFieldCS.hlsl's native-res
	///      write target (BokehCS.hlsl read-modify-writes the same UAV, it
	///      has no resources of its own). Unlike LensFlareIndices this is not
	///      an additive contribution - it is a whole replacement HDR buffer.
	///      When enabled_ is set, LensFlareCS.hlsl/ToneMappingCS.hlsl read
	///      shaderResourceViewIndex_ instead of PostProcessIndices::
	///      sourceColorIndex_.
	/// [JP] ビューごとの被写界深度インデックス/チューニング。
	///      unorderedAccessViewIndex_/shaderResourceViewIndex_ は
	///      DepthOfFieldCS.hlsl のネイティブ解像度書き込み先(BokehCS.hlsl は
	///      同じUAVをread-modify-writeする、自前のリソースは持たない)。
	///      LensFlareIndices と違い加算コントリビューションではなく、HDR
	///      バッファそのものの置き換え。enabled_ が立っている間、
	///      LensFlareCS.hlsl/ToneMappingCS.hlsl は PostProcessIndices::
	///      sourceColorIndex_ ではなく shaderResourceViewIndex_ を読む。
	struct DepthOfFieldIndices
	{
		Uint enabled_ = 0;
		Uint unorderedAccessViewIndex_ = 0;
		Uint shaderResourceViewIndex_ = 0;
		Float focusDistance_ = 0.0f;

		Float focusRange_ = 0.0f;
		Float maxBlurRadius_ = 0.0f;
		Uint depthOfFieldPadding_[2] = { 0, 0 };
	};
	static_assert(sizeof(DepthOfFieldIndices) % 16 == 0, "DepthOfFieldIndices が 16 バイト行の倍数ではありません");

	/// [EN] Per-view bokeh-highlight indices/tuning. BokehCS.hlsl only runs
	///      when this AND DepthOfFieldIndices.enabled_ are both set - it has
	///      no resources of its own, it scatters shaped highlights into
	///      DepthOfFieldIndices' output buffer.
	/// [JP] ビューごとのボケハイライトインデックス/チューニング。
	///      BokehCS.hlsl はこれと DepthOfFieldIndices.enabled_ の両方が
	///      立っている時だけ走る — 自前のリソースは持たず、
	///      DepthOfFieldIndices の出力バッファへ形状付きハイライトを散布する。
	struct BokehIndices
	{
		Uint enabled_ = 0;
		Float highlightThreshold_ = 0.0f;
		Float highlightIntensity_ = 0.0f;
		Uint bladeCount_ = 0;
	};
	static_assert(sizeof(BokehIndices) % 16 == 0, "BokehIndices が 16 バイト行の倍数ではありません");

	/// [EN] Per-view sharpness indices/tuning. SharpnessCS.hlsl runs last,
	///      after ToneMappingCS.hlsl - sourceShaderResourceViewIndex_ is the
	///      bindless SRV of ToneMappingCS.hlsl's tone-mapped output (now an
	///      intermediate buffer), destinationUnorderedAccessViewIndex_ is
	///      this pass's own output, which becomes the new final display
	///      texture (PostProcessRenderer::OutputResource et al. now point at
	///      it). Runs unconditionally every frame like ToneMappingCS.hlsl;
	///      enabled_ just gates whether the shader applies the sharpen
	///      offset or passes the source through unchanged.
	/// [JP] ビューごとのシャープネスインデックス/チューニング。
	///      SharpnessCS.hlsl は ToneMappingCS.hlsl の後、最後に走る -
	///      sourceShaderResourceViewIndex_ は ToneMappingCS.hlsl のトーンマップ
	///      済み出力(今は中間バッファ)の bindless SRV、
	///      destinationUnorderedAccessViewIndex_ はこのパス自身の出力で、
	///      新しい最終表示テクスチャになる(PostProcessRenderer::
	///      OutputResource 等がこちらを指すようになる)。ToneMappingCS.hlsl と
	///      同様に毎フレーム無条件で走る。enabled_ はシェーダがシャープ
	///      オフセットを適用するか、ソースをそのまま通すかを切り替えるだけ。
	struct SharpnessIndices
	{
		Uint sourceShaderResourceViewIndex_ = 0;
		Uint destinationUnorderedAccessViewIndex_ = 0;
		Uint enabled_ = 0;
		Float amount_ = 0.0f;
	};
	static_assert(sizeof(SharpnessIndices) % 16 == 0, "SharpnessIndices が 16 バイト行の倍数ではありません");

	/// [EN] Mirrors Shader/Constants.hlsli's PostProcessIndices. Per-view (not
	///      StructuredIndices) for the same reason the shadow/AO accumulation
	///      chains are: the histogram/persistent-exposure buffers and the
	///      display output texture are genuinely per-camera state. Each
	///      effect gets its own group struct (Exposure/ToneMapping/
	///      LensFlare/DepthOfField/BokehIndices above) instead of flat fields
	///      here, matching the Shadow/AmbientOcclusion/GlobalIllumination/
	///      Dlss grouping below.
	/// [JP] Shader/Constants.hlsli の PostProcessIndices と対応。
	///      StructuredIndices ではなくビューごとに持つ理由は影/AO 蓄積
	///      チェーンと同じ — ヒストグラム/永続露出バッファと表示出力
	///      テクスチャは正真正銘カメラごとの状態だから。各エフェクトは上の
	///      Exposure/ToneMapping/LensFlare/DepthOfField/BokehIndices のような
	///      専用グループ構造体を持つ(ここにフラットなフィールドを並べない) —
	///      下の Shadow/AmbientOcclusion/GlobalIllumination/Dlss と同じ
	///      グループ化。
	struct PostProcessIndices
	{
		Uint outputUnorderedAccessViewIndex_ = 0;
		Uint sourceColorIndex_ = 0;

		/// [EN] Set when the lens stage (chromatic aberration and/or
		///      vignette) ran, in which case lensStageShaderResourceViewIndex_
		///      is the buffer it left the scene in and ToneMappingCS.hlsl must
		///      read that instead of sourceColorIndex_ or the depth-of-field
		///      output. Resolved on the CPU in PrepareView so the shader needs
		///      one branch rather than a chain of them.
		/// [JP] レンズ段(色収差および/またはビネット)が走った時に立つ。
		///      その場合 lensStageShaderResourceViewIndex_ がシーンの置かれた
		///      バッファで、ToneMappingCS.hlsl は sourceColorIndex_ や
		///      被写界深度の出力ではなくそちらを読まなければならない。
		///      PrepareView がCPU側で解決するので、シェーダ側の分岐は
		///      連鎖ではなく1回で済む。
		Uint lensStageEnabled_ = 0;
		Uint lensStageShaderResourceViewIndex_ = 0;

		ExposureIndices exposure_;
		ToneMappingIndices toneMapping_;
		LensFlareIndices lensFlare_;
		LensFlareStreakIndices lensFlareStreak_;
		BloomIndices bloom_;
		AnamorphicFlareIndices anamorphicFlare_;
		ColorGradingIndices colorGrading_;
		LensDistortionIndices lensDistortion_;
		ChromaticAberrationIndices chromaticAberration_;
		VignetteIndices vignette_;
		DepthOfFieldIndices depthOfField_;
		BokehIndices bokeh_;
		SharpnessIndices sharpness_;
		FilmGrainIndices filmGrain_;
	};
	static_assert(sizeof(PostProcessIndices) == 784, "PostProcessIndices が Shader/Constants.hlsli と一致していません");

	/// [EN] Per-view ray-traced shadow accumulation indices — see
	///      Shader/Constants.hlsli for why these are per-view instead of in
	///      StructuredIndices (screen-space signal, one chain per camera;
	///      StructuredIndices is shared by all views).
	/// [JP] ビューごとのレイトレ影蓄積インデックス。StructuredIndices では
	///      なくここに置く理由は Shader/Constants.hlsli 参照(影は
	///      スクリーンスペース信号でカメラごとに1チェーン必要、
	///      StructuredIndices は全ビュー共有のため)。
	struct ShadowAccumulationIndices
	{
		Uint historyShaderResourceViewIndex_ = 0;
		Uint accumulatedUnorderedAccessViewIndex_ = 0;
		Uint visibilityShaderResourceViewIndex_ = 0;
		Uint shadowAccumulationPadding_ = 0;

		Uint atrousScratch0ShaderResourceViewIndex_ = 0;
		Uint atrousScratch0UnorderedAccessViewIndex_ = 0;
		Uint atrousScratch1ShaderResourceViewIndex_ = 0;
		Uint atrousScratch1UnorderedAccessViewIndex_ = 0;
	};
	static_assert(sizeof(ShadowAccumulationIndices) % 16 == 0, "ShadowAccumulationIndices が 16 バイト行の倍数ではありません");

	/// [JP] ビューごとのレイトレAO蓄積インデックス。影と同じ理由でここに置く。
	struct AmbientOcclusionAccumulationIndices
	{
		Uint historyShaderResourceViewIndex_ = 0;
		Uint accumulatedUnorderedAccessViewIndex_ = 0;
		Uint opennessShaderResourceViewIndex_ = 0;
		Uint ambientOcclusionAccumulationPadding_ = 0;
	};
	static_assert(sizeof(AmbientOcclusionAccumulationIndices) % 16 == 0, "AmbientOcclusionAccumulationIndices が 16 バイト行の倍数ではありません");

	/// [JP] ビューごとのレイトレGI蓄積インデックス。影/AOと同じ理由でここに
	///      置く。生の1spp放射輝度(StructuredIndices::globalIllumination_)は
	///      全ビュー共有の単一バッファ — こちらはビューごとのデノイズ
	///      (空間+時間)済み結果で、DeferredLightingPS.hlsl がサンプルする。
	///
	///      atrousScratch0_/atrousScratch1_ は GlobalIlluminationDenoiseCS.hlsl
	///      の ATrousPass1/2/3 エントリポイントが読み書きするビューごとの
	///      ピンポンスクラッチテクスチャ(同ファイル参照)。history_/
	///      accumulated_/radiance_ と違い、こちらは GlobalIlluminationRenderer::
	///      Create/Resize で一度だけ設定する(PrepareFrame では触らない) —
	///      純粋なスクラッチで、A-Trousパス自身が毎回全画素を上書きするため、
	///      bindless インデックスをフレームごとに更新する必要がない。
	struct GlobalIlluminationAccumulationIndices
	{
		Uint historyShaderResourceViewIndex_ = 0;
		Uint accumulatedUnorderedAccessViewIndex_ = 0;
		Uint radianceShaderResourceViewIndex_ = 0;
		Uint globalIlluminationAccumulationPadding_ = 0;

		Uint atrousScratch0ShaderResourceViewIndex_ = 0;
		Uint atrousScratch0UnorderedAccessViewIndex_ = 0;
		Uint atrousScratch1ShaderResourceViewIndex_ = 0;
		Uint atrousScratch1UnorderedAccessViewIndex_ = 0;
	};
	static_assert(sizeof(GlobalIlluminationAccumulationIndices) % 16 == 0, "GlobalIlluminationAccumulationIndices が 16 バイト行の倍数ではありません");

	/// [JP] ビューごとのレイトレ反射蓄積インデックス。GIと同じ理由でここに
	///      置く。生の1spp GGXサンプル放射輝度(StructuredIndices::reflection_)
	///      は全ビュー共有の単一バッファ — こちらはビューごとのデノイズ
	///      (空間+時間)済み結果で、DeferredLightingPS.hlsl がサンプルする。
	struct ReflectionAccumulationIndices
	{
		Uint historyShaderResourceViewIndex_ = 0;
		Uint accumulatedUnorderedAccessViewIndex_ = 0;
		Uint radianceShaderResourceViewIndex_ = 0;
		Uint reflectionAccumulationPadding_ = 0;

		Uint atrousScratch0ShaderResourceViewIndex_ = 0;
		Uint atrousScratch0UnorderedAccessViewIndex_ = 0;
		Uint atrousScratch1ShaderResourceViewIndex_ = 0;
		Uint atrousScratch1UnorderedAccessViewIndex_ = 0;
	};
	static_assert(sizeof(ReflectionAccumulationIndices) % 16 == 0, "ReflectionAccumulationIndices が 16 バイト行の倍数ではありません");

	/// [EN] DLSS Ray Reconstruction's synthesized RGB=normal/A=roughness
	///      buffer for this view. Genuinely per-view (Editor's and Game's
	///      G-Buffer content differ) - set once at
	///      DlssRayReconstructionRenderer::Create() time (the resource is
	///      single-buffered per view, never ping-ponged, so unlike the
	///      accumulation chains above there is no per-frame PrepareFrame-style
	///      update needed).
	/// [JP] このビュー用に合成した DLSS Ray Reconstruction の RGB=法線/
	///      A=ラフネスバッファ。正真正銘ビューごと(Editor/Game で G-Buffer の
	///      内容が異なる)。DlssRayReconstructionRenderer::Create() 時点で
	///      1度だけ設定する(このリソースはビューごとの単一バッファでピンポン
	///      無しのため、上の蓄積チェーン群と違って毎フレームの
	///      PrepareFrame相当の更新は不要)。
	struct DlssIndices
	{
		Uint normalRoughnessUnorderedAccessViewIndex_ = 0;
		Uint specularAlbedoUnorderedAccessViewIndex_ = 0;
		Uint diffuseAlbedoUnorderedAccessViewIndex_ = 0;
		Uint dlssPadding_ = 0;
	};
	static_assert(sizeof(DlssIndices) % 16 == 0, "DlssIndices が 16 バイト行の倍数ではありません");

	struct ConstantIndices
	{
		Uint sceneIndex_ = 0;
		Uint lightIndex_ = 0;
		Uint clusterConstantIndex_ = 0;
		Uint viewMode_ = 0;

		ShadowAccumulationIndices shadow_;
		AmbientOcclusionAccumulationIndices ambientOcclusion_;
		GlobalIlluminationAccumulationIndices globalIllumination_;
		ReflectionAccumulationIndices reflection_;
		DlssIndices dlss_;

		PostProcessIndices postProcess_;
	};
	static_assert(sizeof(ConstantIndices) == 58 * 16, "ConstantIndices が Shader/Constants.hlsli と一致していません");

	/// [EN] Mirrors Shader/Structured.hlsli. Each group is a whole number of
	///      16-byte cbuffer rows with its padding written out explicitly, and
	///      every group carries a static_assert so a mismatch is a compile
	///      error instead of shaders silently reading shifted fields.
	///      Add fields INSIDE a group and adjust that group's padding.
	/// [JP] Shader/Structured.hlsli と対応。各グループはきっちり 16 バイト行の
	///      倍数で、パディングも明示している。グループごとに static_assert を
	///      置いてあるので、ずれた瞬間にコンパイルエラーになる(黙って全シェーダ
	///      が化けた値を読む事態にならない)。フィールドを足すときはグループの
	///      【中】に足し、そのグループのパディングで調整すること。
	struct SpriteIndices
	{
		Uint imageIndex_ = 0;
		Uint imageBillboardIndex_ = 0;
		Uint fontIndex_ = 0;
		Uint fontBillboardIndex_ = 0;
	};
	static_assert(sizeof(SpriteIndices) % 16 == 0, "SpriteIndices が 16 バイト行の倍数ではありません");

	struct ModelIndices
	{
		Uint instanceIndex_ = 0;
		Uint boneMatrixIndex_ = 0;
		Uint hiZIndex_ = 0;
		Uint selectionMaskIndex_ = 0;
	};
	static_assert(sizeof(ModelIndices) % 16 == 0, "ModelIndices が 16 バイト行の倍数ではありません");

	struct OitIndices
	{
		Uint headPointerIndex_ = 0;
		Uint fragmentBufferIndex_ = 0;
		Uint counterIndex_ = 0;
		Uint oitPadding_ = 0;
	};
	static_assert(sizeof(OitIndices) % 16 == 0, "OitIndices が 16 バイト行の倍数ではありません");

	struct GBufferIndices
	{
		/// [EN] RT0: base_color.rgb + metallic.
		/// [JP] RT0: base_color.rgb + metallic。
		Uint index0_ = 0;
		/// [EN] RT1: octNormal.rg + roughness (.a unused).
		/// [JP] RT1: octNormal.rg + roughness(.a は未使用)。
		Uint index1_ = 0;
		/// [EN] RT2: velocity.
		/// [JP] RT2: velocity。
		Uint index2_ = 0;
		/// [EN] RT3: emissive.rgb (raw - emissive_strength_ applied at lighting time).
		/// [JP] RT3: emissive.rgb(生値。emissive_strength_ はライティング時に適用)。
		Uint index3_ = 0;
		/// [EN] RT4: VisibilityBuffer id (instance/meshlet/triangle), read by
		///      the material resolve pass (Model/MaterialResolveCS.hlsl) and
		///      by DeferredLightingPS.hlsl for KHR extension lookups.
		/// [JP] RT4: VisibilityBuffer id(instance/meshlet/triangle)。マテリアル
		///      解決パス(Model/MaterialResolveCS.hlsl)と、KHR拡張参照のため
		///      DeferredLightingPS.hlsl が読む。
		Uint index4_ = 0;
		Uint depthIndex_ = 0;
		/// [EN] RT2's UAV. Written wholesale by the material resolve pass, and
		///      patched again for background (sky/cloud) pixels by
		///      DLSS/DlssBackgroundVelocityCS.hlsl (DLSS-RR only).
		/// [JP] RT2のUAV。マテリアル解決パスが丸ごと書き、DLSS-RR時のみ背景
		///      (空/雲)ピクセルを DLSS/DlssBackgroundVelocityCS.hlsl が
		///      追加でパッチする。
		Uint velocityUnorderedAccessViewIndex_ = 0;
		/// [EN] UAV indices onto RT0/1/3, written wholesale by the material
		///      resolve pass (RT2's UAV is velocityUnorderedAccessViewIndex_ above).
		/// [JP] RT0/1/3 への UAV インデックス。マテリアル解決パスが丸ごと書く
		///      (RT2 の UAV は上の velocityUnorderedAccessViewIndex_)。
		Uint index0UnorderedAccessViewIndex_ = 0;
		Uint index1UnorderedAccessViewIndex_ = 0;
		Uint index3UnorderedAccessViewIndex_ = 0;
		Uint gbufferPadding0_ = 0;
		Uint gbufferPadding1_ = 0;
	};
	static_assert(sizeof(GBufferIndices) % 16 == 0, "GBufferIndices が 16 バイト行の倍数ではありません");

	struct MaterialSortIndices
	{
		/// [EN] Per-bucket pixel count -> exclusive-scan offset -> atomic write
		///      cursor (Model/MaterialClassifyCS.hlsl / MaterialPrefixSumCS.hlsl /
		///      MaterialScatterCS.hlsl), RWStructuredBuffer<uint>[MATERIAL_SORT_BUCKET_COUNT].
		/// [JP] バケットごとのピクセル数 → 排他的スキャンのオフセット →
		///      atomic書き込みカーソル(Model/MaterialClassifyCS.hlsl /
		///      MaterialPrefixSumCS.hlsl / MaterialScatterCS.hlsl)。
		///      RWStructuredBuffer<uint>[MATERIAL_SORT_BUCKET_COUNT]。
		Uint bucketIndex_ = 0;
		/// [EN] Material-sorted pixel list, RWStructuredBuffer<uint>[width*height].
		///      Model/MaterialResolveCS.hlsl dispatches 1D over this instead of
		///      raw screen order.
		/// [JP] マテリアルでソート済みのピクセルリスト、
		///      RWStructuredBuffer<uint>[width*height]。Model/MaterialResolveCS.hlsl
		///      はスクリーン順ではなくこれを1Dディスパッチで辿る。
		Uint sortedPixelListIndex_ = 0;
		Uint materialSortPadding0_ = 0;
		Uint materialSortPadding1_ = 0;
	};
	static_assert(sizeof(MaterialSortIndices) % 16 == 0, "MaterialSortIndices が 16 バイト行の倍数ではありません");

	struct SkyIndices
	{
		Uint environmentCubeIndex_ = 0;
		Uint diffuseIrradianceIndex_ = 0;
		Uint specularPrefilteredIndex_ = 0;
		Uint brdfLutIndex_ = 0;
		Float intensity_ = 1.0f;
		Uint skyPadding_[3] = { 0, 0, 0 };
	};
	static_assert(sizeof(SkyIndices) % 16 == 0, "SkyIndices が 16 バイト行の倍数ではありません");

	/// [EN] Shared by every ray-traced pass. instanceDataIndex_ is the
	///      per-TLAS-instance geometry/material table: reflection uploads it,
	///      GI reads the same one (same TLAS, same instance order).
	/// [JP] 全レイトレパス共通。instanceDataIndex_ は TLAS インスタンスごとの
	///      ジオメトリ/マテリアルテーブルで、反射がアップロードしたものを
	///      GI も読む(同じ TLAS・同じ順序)。
	struct RaytracingIndices
	{
		Uint tlasIndex_ = 0;
		Uint instanceDataIndex_ = 0;
		Uint raytracingPadding_[2] = { 0, 0 };
	};
	static_assert(sizeof(RaytracingIndices) % 16 == 0, "RaytracingIndices が 16 バイト行の倍数ではありません");

	struct ShadowIndices
	{
		Uint rawVisibilityUnorderedAccessViewIndex_ = 0;
		Uint rawVisibilityShaderResourceViewIndex_ = 0;
		Uint rayConstantIndex_ = 0;
		Uint shadowPadding_ = 0;
	};
	static_assert(sizeof(ShadowIndices) % 16 == 0, "ShadowIndices が 16 バイト行の倍数ではありません");

	struct AmbientOcclusionIndices
	{
		Uint rawUnorderedAccessViewIndex_ = 0;
		Uint rawShaderResourceViewIndex_ = 0;
		Uint rayConstantIndex_ = 0;
		Uint ambientOcclusionPadding_ = 0;
	};
	static_assert(sizeof(AmbientOcclusionIndices) % 16 == 0, "AmbientOcclusionIndices が 16 バイト行の倍数ではありません");

	struct SubsurfaceScatteringIndices
	{
		Uint transmittanceUnorderedAccessViewIndex_ = 0;
		Uint transmittanceShaderResourceViewIndex_ = 0;
		Uint rayConstantIndex_ = 0;
		Uint subsurfaceScatteringPadding_ = 0;
	};
	static_assert(sizeof(SubsurfaceScatteringIndices) % 16 == 0, "SubsurfaceScatteringIndices が 16 バイト行の倍数ではありません");

	struct ReflectionIndices
	{
		Uint outputUnorderedAccessViewIndex_ = 0;
		Uint outputShaderResourceViewIndex_ = 0;
		Uint rayConstantIndex_ = 0;
		Uint reflectionPadding_ = 0;
	};
	static_assert(sizeof(ReflectionIndices) % 16 == 0, "ReflectionIndices が 16 バイト行の倍数ではありません");

	struct RefractionIndices
	{
		Uint outputUnorderedAccessViewIndex_ = 0;
		Uint outputShaderResourceViewIndex_ = 0;
		Uint rayConstantIndex_ = 0;
		Uint refractionPadding_ = 0;
	};
	static_assert(sizeof(RefractionIndices) % 16 == 0, "RefractionIndices が 16 バイト行の倍数ではありません");

	struct GlobalIlluminationIndices
	{
		Uint outputUnorderedAccessViewIndex_ = 0;
		Uint outputShaderResourceViewIndex_ = 0;
		Uint rayConstantIndex_ = 0;
		Uint globalIlluminationPadding_ = 0;
	};
	static_assert(sizeof(GlobalIlluminationIndices) % 16 == 0, "GlobalIlluminationIndices が 16 バイト行の倍数ではありません");

	struct CloudIndices
	{
		Uint outputUnorderedAccessViewIndex_ = 0;
		Uint outputShaderResourceViewIndex_ = 0;
		Uint rayConstantIndex_ = 0;
		Uint shapeNoiseUnorderedAccessViewIndex_ = 0;
		Uint shapeNoiseShaderResourceViewIndex_ = 0;
		Uint detailNoiseUnorderedAccessViewIndex_ = 0;
		Uint detailNoiseShaderResourceViewIndex_ = 0;
		Uint cloudPadding_ = 0;
	};
	static_assert(sizeof(CloudIndices) % 16 == 0, "CloudIndices が 16 バイト行の倍数ではありません");

	struct StarIndices
	{
		Uint outputUnorderedAccessViewIndex_ = 0;
		Uint outputShaderResourceViewIndex_ = 0;
		Uint rayConstantIndex_ = 0;
		Uint starPadding_ = 0;
	};
	static_assert(sizeof(StarIndices) % 16 == 0, "StarIndices が 16 バイト行の倍数ではありません");

	struct WeatherParticleIndices
	{
		Uint rainParticleUnorderedAccessViewIndex_ = 0;
		Uint rainParticleShaderResourceViewIndex_ = 0;
		Uint snowParticleUnorderedAccessViewIndex_ = 0;
		Uint snowParticleShaderResourceViewIndex_ = 0;
		Uint rayConstantIndex_ = 0;
		Uint weatherParticlePadding_[3] = { 0, 0, 0 };
	};
	static_assert(sizeof(WeatherParticleIndices) % 16 == 0, "WeatherParticleIndices が 16 バイト行の倍数ではありません");

	struct VolumetricLightIndices
	{
		Uint densityUnorderedAccessViewIndex_ = 0;
		Uint scatteringUnorderedAccessViewIndex_ = 0;
		Uint integrationUnorderedAccessViewIndex_ = 0;
		Uint integrationShaderResourceViewIndex_ = 0;
		Uint rayConstantIndex_ = 0;
		Uint volumetricLightPadding_[3] = { 0, 0, 0 };
	};
	static_assert(sizeof(VolumetricLightIndices) % 16 == 0, "VolumetricLightIndices が 16 バイト行の倍数ではありません");

	struct MovieIndices
	{
		Uint spriteIndex_ = 0;
		Uint billboardIndex_ = 0;
		Uint fullscreenIndex_ = 0;
		Uint moviePadding_ = 0;
	};
	static_assert(sizeof(MovieIndices) % 16 == 0, "MovieIndices が 16 バイト行の倍数ではありません");

	struct StructuredIndices
	{
		SpriteIndices sprite_;
		ModelIndices model_;
		OitIndices oit_;
		GBufferIndices gbuffer_;
		MaterialSortIndices materialSort_;
		SkyIndices sky_;
		RaytracingIndices raytracing_;
		ShadowIndices shadow_;
		AmbientOcclusionIndices ambientOcclusion_;
		SubsurfaceScatteringIndices subsurfaceScattering_;
		ReflectionIndices reflection_;
		RefractionIndices refraction_;
		GlobalIlluminationIndices globalIllumination_;
		CloudIndices cloud_;
		StarIndices star_;
		WeatherParticleIndices weatherParticle_;
		VolumetricLightIndices volumetricLight_;
		MovieIndices movie_;
	};
	static_assert(sizeof(StructuredIndices) % 16 == 0, "StructuredIndices が 16 バイト行の倍数ではありません");

	class BindlessHeap;

	class IndicesSystem
	{
	public:
		IndicesSystem(ID3D12Device* device, BindlessHeap* heap);
		~IndicesSystem() = default;

		void UploadEditor();

		void UploadGame();

		void UploadCanvas();

		D3D12_GPU_VIRTUAL_ADDRESS EditorConstantAddress()const;

		D3D12_GPU_VIRTUAL_ADDRESS GameConstantAddress()const;

		D3D12_GPU_VIRTUAL_ADDRESS CanvasConstantAddress()const;

		D3D12_GPU_VIRTUAL_ADDRESS StructuredAddress()const;

	public:
		void SetEditorSceneIndex(Uint index);

		void SetGameSceneIndex(Uint index);

		void SetCanvasSceneIndex(Uint index);

		void SetLightIndex(Uint index);

		void SetClusterConstantIndex(Uint index);

		void SetEditorViewMode(Uint mode);

	public:
		void SetImageSpriteIndex(Uint index);

		void SetImageBillboardIndex(Uint index);

		void SetFontSpriteIndex(Uint index);

		void SetFontBillboardIndex(Uint index);

		void SetModelInstanceIndex(Uint index);

		void SetModelBoneMatrixIndex(Uint index);

		void SetOITHeadPointerIndex(Uint index);

		void SetOITFragmentBufferIndex(Uint index);

		void SetOITCounterIndex(Uint index);

		void SetHiZIndex(Uint index);

		void SetGBuffer0Index(Uint index);

		void SetGBuffer1Index(Uint index);

		void SetGBuffer2Index(Uint index);

		void SetGBuffer3Index(Uint index);

		void SetGBuffer4Index(Uint index);

		void SetGBufferDepthIndex(Uint index);

		void SetGBufferVelocityUnorderedAccessViewIndex(Uint index);

		void SetGBuffer0UnorderedAccessViewIndex(Uint index);

		void SetGBuffer1UnorderedAccessViewIndex(Uint index);

		void SetGBuffer3UnorderedAccessViewIndex(Uint index);

		void SetMaterialSortBucketIndex(Uint index);

		void SetMaterialSortedPixelListIndex(Uint index);

		void SetSkyEnvironmentCubeIndex(Uint index);

		void SetSkyDiffuseIrradianceIndex(Uint index);

		void SetSkySpecularPrefilteredIndex(Uint index);

		void SetSkyBrdfLutIndex(Uint index);

		void SetSkyIntensity(Float intensity);

		void SetSelectionMaskIndex(Uint index);

		void SetTLASIndex(Uint index);

		void SetShadowRawVisibilityUnorderedAccessViewIndex(Uint index);

		void SetShadowRawVisibilityShaderResourceViewIndex(Uint index);

		void SetShadowRayConstantIndex(Uint index);

		/// [EN] Registers one view's shadow-accumulation chain into that
		///      view's ConstantIndices. Canvas mirrors the editor values so
		///      the fields are never left undefined (canvas never samples
		///      them).
		/// [JP] 指定ビューの影蓄積チェーンをそのビューの ConstantIndices へ
		///      登録する。Canvas はエディタと同値を入れて未定義を避ける
		///      (Canvas がこれらを読むことはない)。
		void SetEditorShadowIndices(Uint historyShaderResourceViewIndex, Uint accumulatedUnorderedAccessViewIndex, Uint visibilityShaderResourceViewIndex);

		void SetGameShadowIndices(Uint historyShaderResourceViewIndex, Uint accumulatedUnorderedAccessViewIndex, Uint visibilityShaderResourceViewIndex);

		void SetEditorShadowAtrousScratchIndices(Uint scratch0ShaderResourceViewIndex, Uint scratch0UnorderedAccessViewIndex, Uint scratch1ShaderResourceViewIndex, Uint scratch1UnorderedAccessViewIndex);

		void SetGameShadowAtrousScratchIndices(Uint scratch0ShaderResourceViewIndex, Uint scratch0UnorderedAccessViewIndex, Uint scratch1ShaderResourceViewIndex, Uint scratch1UnorderedAccessViewIndex);

		void SetAmbientOcclusionRawUnorderedAccessViewIndex(Uint index);

		void SetAmbientOcclusionRawShaderResourceViewIndex(Uint index);

		void SetAmbientOcclusionRayConstantIndex(Uint index);

		void SetEditorAmbientOcclusionIndices(Uint historyShaderResourceViewIndex, Uint accumulatedUnorderedAccessViewIndex, Uint opennessShaderResourceViewIndex);

		void SetGameAmbientOcclusionIndices(Uint historyShaderResourceViewIndex, Uint accumulatedUnorderedAccessViewIndex, Uint opennessShaderResourceViewIndex);

		void SetEditorGlobalIlluminationAccumulationIndices(Uint historyShaderResourceViewIndex, Uint accumulatedUnorderedAccessViewIndex, Uint radianceShaderResourceViewIndex);

		void SetGameGlobalIlluminationAccumulationIndices(Uint historyShaderResourceViewIndex, Uint accumulatedUnorderedAccessViewIndex, Uint radianceShaderResourceViewIndex);

		/// [EN] Registers the A-Trous ping-pong scratch textures' bindless
		///      indices for one view. Called once from
		///      GlobalIlluminationRenderer::Create/Resize (NOT PrepareFrame) —
		///      see GlobalIlluminationAccumulationIndices' comment for why.
		/// [JP] 1ビュー分の A-Trous ピンポンスクラッチテクスチャの bindless
		///      インデックスを登録する。GlobalIlluminationRenderer::Create/Resize
		///      から一度だけ呼ぶ(PrepareFrame からではない) — 理由は
		///      GlobalIlluminationAccumulationIndices のコメント参照。
		void SetEditorGlobalIlluminationAtrousScratchIndices(Uint scratch0ShaderResourceViewIndex, Uint scratch0UnorderedAccessViewIndex, Uint scratch1ShaderResourceViewIndex, Uint scratch1UnorderedAccessViewIndex);

		void SetGameGlobalIlluminationAtrousScratchIndices(Uint scratch0ShaderResourceViewIndex, Uint scratch0UnorderedAccessViewIndex, Uint scratch1ShaderResourceViewIndex, Uint scratch1UnorderedAccessViewIndex);

		void SetEditorReflectionAccumulationIndices(Uint historyShaderResourceViewIndex, Uint accumulatedUnorderedAccessViewIndex, Uint radianceShaderResourceViewIndex);

		void SetGameReflectionAccumulationIndices(Uint historyShaderResourceViewIndex, Uint accumulatedUnorderedAccessViewIndex, Uint radianceShaderResourceViewIndex);

		void SetEditorReflectionAtrousScratchIndices(Uint scratch0ShaderResourceViewIndex, Uint scratch0UnorderedAccessViewIndex, Uint scratch1ShaderResourceViewIndex, Uint scratch1UnorderedAccessViewIndex);

		void SetGameReflectionAtrousScratchIndices(Uint scratch0ShaderResourceViewIndex, Uint scratch0UnorderedAccessViewIndex, Uint scratch1ShaderResourceViewIndex, Uint scratch1UnorderedAccessViewIndex);

		/// [EN] Registers one view's full post-process payload (resource
		///      indices + tuning scalars) into that view's ConstantIndices.
		///      Takes the whole PostProcessIndices struct rather than a long
		///      scalar parameter list (unlike the 3-scalar shadow/AO setters
		///      above) since it already exists as exactly this payload's shape
		///      and PostProcessRenderer builds one wholesale each frame.
		/// [JP] 指定ビューのポストプロセス一式(リソースインデックス+
		///      チューニング用スカラー)をそのビューの ConstantIndices へ登録
		///      する。上の影/AO(スカラー3つ)と違い構造体をまるごと受け取る —
		///      この構造体自体がまさにこのペイロードの形として存在し、
		///      PostProcessRenderer が毎フレーム1つ組み立てるため。
		void SetEditorPostProcessIndices(const PostProcessIndices& values);

		void SetGamePostProcessIndices(const PostProcessIndices& values);

		void SetSubsurfaceScatteringTransmittanceUnorderedAccessViewIndex(Uint index);

		void SetSubsurfaceScatteringTransmittanceShaderResourceViewIndex(Uint index);

		void SetSubsurfaceScatteringRayConstantIndex(Uint index);

		void SetReflectionOutputUnorderedAccessViewIndex(Uint index);

		void SetReflectionOutputShaderResourceViewIndex(Uint index);

		void SetReflectionRayConstantIndex(Uint index);

		void SetReflectionInstanceDataIndex(Uint index);

		void SetRefractionOutputUnorderedAccessViewIndex(Uint index);

		void SetRefractionOutputShaderResourceViewIndex(Uint index);

		void SetRefractionRayConstantIndex(Uint index);

		void SetGlobalIlluminationOutputUnorderedAccessViewIndex(Uint index);

		void SetGlobalIlluminationOutputShaderResourceViewIndex(Uint index);

		void SetGlobalIlluminationRayConstantIndex(Uint index);

		void SetCloudOutputUnorderedAccessViewIndex(Uint index);

		void SetCloudOutputShaderResourceViewIndex(Uint index);

		void SetCloudRayConstantIndex(Uint index);

		void SetCloudShapeNoiseUnorderedAccessViewIndex(Uint index);

		void SetCloudShapeNoiseShaderResourceViewIndex(Uint index);

		void SetCloudDetailNoiseUnorderedAccessViewIndex(Uint index);

		void SetCloudDetailNoiseShaderResourceViewIndex(Uint index);

		void SetStarOutputUnorderedAccessViewIndex(Uint index);

		void SetStarOutputShaderResourceViewIndex(Uint index);

		void SetStarRayConstantIndex(Uint index);

		void SetRainParticleUnorderedAccessViewIndex(Uint index);

		void SetRainParticleShaderResourceViewIndex(Uint index);

		void SetSnowParticleUnorderedAccessViewIndex(Uint index);

		void SetSnowParticleShaderResourceViewIndex(Uint index);

		void SetWeatherParticleRayConstantIndex(Uint index);

		void SetVolumetricLightDensityUnorderedAccessViewIndex(Uint index);

		void SetVolumetricLightScatteringUnorderedAccessViewIndex(Uint index);

		void SetVolumetricLightIntegrationUnorderedAccessViewIndex(Uint index);

		void SetVolumetricLightIntegrationShaderResourceViewIndex(Uint index);

		void SetVolumetricLightRayConstantIndex(Uint index);

		void SetMovieSpriteIndex(Uint index);

		void SetMovieBillboardIndex(Uint index);

		void SetMovieFullscreenIndex(Uint index);

		void SetEditorDlssNormalRoughnessUnorderedAccessViewIndex(Uint index);

		void SetGameDlssNormalRoughnessUnorderedAccessViewIndex(Uint index);

		void SetEditorDlssSpecularAlbedoUnorderedAccessViewIndex(Uint index);

		void SetGameDlssSpecularAlbedoUnorderedAccessViewIndex(Uint index);

		void SetEditorDlssDiffuseAlbedoUnorderedAccessViewIndex(Uint index);

		void SetGameDlssDiffuseAlbedoUnorderedAccessViewIndex(Uint index);

	private:
		ConstantIndices editorConstantIndices_{};
		ConstantIndices gameConstantIndices_{};
		ConstantIndices canvasConstantIndices_{};

		StructuredIndices structuredIndices_{};

		ResourcePtr<ConstantBuffer<ConstantIndices>> editorConstantIndicesBuffer_;
		ResourcePtr<ConstantBuffer<ConstantIndices>> gameConstantIndicesBuffer_;
		ResourcePtr<ConstantBuffer<ConstantIndices>> canvasConstantIndicesBuffer_;

		ResourcePtr<ConstantBuffer<StructuredIndices>> structuredIndicesBuffer_;
	};
}
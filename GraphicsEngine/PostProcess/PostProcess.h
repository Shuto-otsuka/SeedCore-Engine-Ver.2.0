#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	/**
	* [EN]
	* Depth-of-field settings: a CoC (circle of confusion) gather blur based
	* on the distance from focusDistance_ - pixels within focusRange_ of the
	* focus plane stay sharp, pixels further away blur up to maxBlurRadius_
	* (DepthOfFieldCS.hlsl). Runs at native resolution and writes a whole new
	* HDR buffer (not an additive contribution like LensFlareSettings) that
	* downstream passes (BokehSettings, LensFlareSettings, ToneMappingSettings)
	* read from instead of the raw scene color when enabled_ is set - see
	* PostProcessRenderer::Dispatch for the source-selection branch.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 被写界深度設定: focusDistance_ からの距離に基づく CoC(錯乱円)ギャザー
	* ブラー。焦点面から focusRange_ 以内のピクセルはシャープなまま、それより
	* 遠いピクセルは maxBlurRadius_ まで滲む(DepthOfFieldCS.hlsl)。ネイティブ
	* 解像度で走り、LensFlareSettings のような加算コントリビューションではなく
	* 新しいHDRバッファそのものを書き込む — enabled_ が立っている間、後続パス
	* (BokehSettings、LensFlareSettings、ToneMappingSettings)は生のシーン色
	* ではなくこちらを読む(ソース選択の分岐は PostProcessRenderer::Dispatch
	* 参照)。
	*/
	struct DepthOfFieldSettings
	{
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = false;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("焦点距離", 0.0f, 100.0f)
		Float focusDistance_ = 10.0f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("焦点範囲", 0.01f, 50.0f)
		Float focusRange_ = 4.0f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("最大ぼけ半径", 0.0f, 0.05f)
		Float maxBlurRadius_ = 0.015f;
	};

	/**
	* [EN]
	* Bokeh highlight settings, layered on top of DepthOfFieldSettings'
	* gather blur (BokehCS.hlsl runs after DepthOfFieldCS.hlsl and
	* read-modify-writes the same output buffer - it has no resources of its
	* own). Bright-passes the ORIGINAL scene color (before blur, so highlight
	* positions stay crisp) at highlightThreshold_ and, for pixels with a
	* non-trivial circle of confusion, scatters a bladeCount_-sided polygon
	* highlight (approximated the same "sample along N fixed arms" way
	* LensFlareCS.hlsl does) scaled by that pixel's CoC and boosted by
	* highlightIntensity_ - the classic "bright out-of-focus points become
	* visible bokeh shapes" look. Meaningless without DepthOfFieldSettings.
	* enabled_ also being on; PostProcessRenderer::Dispatch enforces that at
	* runtime.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボケハイライト設定。DepthOfFieldSettings のギャザーブラーの上に重ねる
	* (BokehCS.hlsl は DepthOfFieldCS.hlsl の後に走り、同じ出力バッファを
	* read-modify-write する — 自前のリソースは持たない)。ぼかす前の元の
	* シーン色を highlightThreshold_ でブライトパスし(ハイライト位置が
	* にじまないようにするため)、錯乱円が無視できないピクセルについて
	* bladeCount_ 角形のハイライトを(LensFlareCS.hlsl と同じ「N本の固定腕に
	* 沿ってサンプルする」近似で)そのピクセルのCoCでスケールし
	* highlightIntensity_ で強調して散布する — 「ピントの外れた明るい点が
	* 目に見えるボケ形状になる」典型的な見た目。DepthOfFieldSettings.enabled_
	* も同時に有効でなければ意味を持たない — これは実行時に
	* PostProcessRenderer::Dispatch が担保する。
	*/
	struct BokehSettings
	{
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = false;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("しきい値", 0.0f, 20.0f)
		Float highlightThreshold_ = 4.0f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("強度", 0.0f, 5.0f)
		Float highlightIntensity_ = 1.0f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("角形の頂点数", 3, 8)
		Uint32 bladeCount_ = 6;
	};

	/**
	* [EN]
	* Lens distortion settings, following the radial half of the
	* Brown-Conrady model: a point at radius r from the optical axis is
	* displaced to r * (1 + k1*r^2 + k2*r^4 + k3*r^6). k1 dominates, k2
	* refines the corners, k3 barely moves anything - which is exactly why
	* they are ordered that way in the inspector. Positive values bow the
	* image outward (barrel/fisheye), negative pull it inward (pincushion).
	*
	* Brown-Conrady also has TANGENTIAL terms (p1, p2) that model lens
	* elements being decentred or tilted relative to the axis. They are
	* deliberately not exposed here: that is a manufacturing defect rather
	* than a look anyone reaches for, and it produces an asymmetric skew that
	* mostly reads as a mistake.
	*
	* scale_ exists because barrel distortion pulls the image away from the
	* corners and leaves them empty. Scaling up before distorting crops back
	* into valid pixels; without it, positive k1 shows black corners.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レンズ歪曲設定。Brown-Conrady モデルの半径方向の項に従う: 光軸から
	* 半径 r の点が r * (1 + k1*r^2 + k2*r^4 + k3*r^6) へ変位する。k1 が
	* 支配的で、k2 が四隅を微調整し、k3 はほとんど動かさない — インスペクタ
	* での並び順がそうなっているのはそのため。正の値で外側へ膨らみ(樽型/
	* 魚眼)、負の値で内側へ引き込まれる(糸巻き型)。
	*
	* Brown-Conrady には【接線方向】の項(p1, p2)もあり、レンズ素子が光軸に
	* 対して偏心・傾斜している状態をモデル化する。ここでは意図的に公開して
	* いない: これは製造上の欠陥であって狙って使う画作りではなく、非対称な
	* 歪みになるので大抵は単なる失敗に見えるため。
	*
	* scale_ があるのは、樽型歪曲が像を四隅から引き離して余白を作るから。
	* 歪ませる前に拡大しておけば有効な画素の内側へ収まる — 無いと k1 が
	* 正の時に四隅が黒くなる。
	*/
	struct LensDistortionSettings
	{
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = false;

		/// [EN] Primary radial coefficient - positive is barrel, negative is
		///      pincushion. This is the one to reach for first.
		/// [JP] 半径方向の主係数。正で樽型、負で糸巻き型。まず触るのはここ。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("歪曲 k1", -0.5f, 0.5f)
		Float k1_ = 0.0f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("歪曲 k2", -0.5f, 0.5f)
		Float k2_ = 0.0f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("歪曲 k3", -0.5f, 0.5f)
		Float k3_ = 0.0f;

		/// [EN] Zoom applied before distorting, to crop away the empty
		///      corners barrel distortion creates.
		/// [JP] 歪ませる前に掛ける拡大。樽型歪曲が作る四隅の余白を
		///      切り落とすため。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("スケール", 0.5f, 1.5f)
		Float scale_ = 1.0f;
	};

	/**
	* [EN]
	* Chromatic aberration settings. Models LATERAL (transverse) chromatic
	* aberration specifically: the lens magnifies each wavelength slightly
	* differently, so a point images at a slightly different radius per
	* colour. That is why the fringing is zero at the optical centre and
	* grows toward the corners - the shader offsets its samples along the
	* radial direction, scaled by distance from centre. The other kind,
	* LONGITUDINAL (axial) aberration, focuses wavelengths at different
	* DEPTHS and shows up as purple/green haloes on out-of-focus highlights
	* uniformly across the frame; it is not modelled here, since it needs
	* per-wavelength focus and belongs with depth of field.
	*
	* sampleCount_ exists because sampling only R/G/B at three offsets
	* produces three visibly separated coloured copies rather than a fringe.
	* Marching more samples along the same radial line and weighting each by
	* a smooth spectral response blends them into a continuous rainbow edge -
	* the same reason Unity's implementation drives its sample count from
	* intensity and feeds it through a spectral LUT. The response here is
	* computed procedurally instead of sampling a LUT texture, so no asset is
	* needed, and it is normalised so the weights sum to white and the effect
	* does not tint the image overall.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 色収差設定。モデル化しているのは【倍率色収差(横方向)】: レンズが波長
	* ごとにわずかに異なる倍率で結像するため、1点が色ごとに少しずつ違う
	* 半径に写る。これが「光軸中心では色ずれが0で、四隅へ向かうほど大きく
	* なる」理由で、シェーダも半径方向へ、中心からの距離に比例した
	* オフセットでサンプルする。もう一方の【軸上色収差(縦方向)】は波長ごとに
	* 合焦【距離】がずれるもので、ピントの外れたハイライトに画面全域で
	* 一様に紫/緑の縁として出る。こちらは波長ごとの合焦が必要で被写界深度の
	* 領分なので、ここでは扱わない。
	*
	* sampleCount_ があるのは、R/G/B の3点だけをずらすと「3つの色の分身」が
	* はっきり見えてしまい、縁のにじみにならないため。同じ半径方向の線上を
	* より多くのサンプルで刻み、それぞれに滑らかなスペクトル応答で重みを
	* 付けると、連続した虹色の縁に溶ける — Unity の実装がサンプル数を強度から
	* 決めてスペクトルLUTに通しているのと同じ理由。ここでは LUT テクスチャを
	* 引かず手続き的に応答を計算するのでアセットが不要で、重みの合計が白に
	* なるよう正規化してあるため効果全体で色被りしない。
	*/
	struct ChromaticAberrationSettings
	{
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = false;

		/// [EN] Maximum radial sample offset at the screen corners, in UV.
		/// [JP] 画面四隅における半径方向の最大サンプルオフセット(UV単位)。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("強度", 0.0f, 0.05f)
		Float intensity_ = 0.008f;

		/// [EN] Samples marched along the radial line. Below about 6 the
		///      fringe breaks into separate coloured copies.
		/// [JP] 半径方向の線上を刻むサンプル数。6程度を下回ると、にじみでは
		///      なく分離した色の分身に見え始める。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("サンプル数", 3, 16)
		Uint32 sampleCount_ = 8;
	};

	/**
	* [EN]
	* Vignette settings. The default exponent_ of 4 is not an arbitrary
	* number: natural vignetting follows the cosine-fourth law, where
	* illumination at the sensor falls off as cos^4 of the angle off the
	* optical axis. That falloff is intrinsic to every lens - it comes from
	* the projected areas of the pupil and the pixel plus the inverse-square
	* law, so unlike optical vignetting (elements shading each other, curable
	* by stopping down) or mechanical vignetting (a hood clipping the corners)
	* it cannot be designed away. Leaving exponent_ at 4 gives the physical
	* falloff; raising or lowering it is pure art direction.
	*
	* Applied before exposure and the tone curve, because vignetting is light
	* that never reached the sensor - it has to be part of what gets exposed,
	* not a darkening painted over the exposed result.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ビネット設定。既定の exponent_ = 4 は恣意的な数字ではない: 自然
	* ビネットは【コサイン4乗則】に従い、センサー面の照度が光軸からの角度の
	* cos^4 で落ちる。この減光は瞳と画素の投影面積および逆二乗則から来る
	* もので、あらゆるレンズに内在する — 光学ビネット(素子同士の遮蔽、
	* 絞れば解消する)や機械ビネット(フードによる四隅のケラレ)と違い、
	* 設計で無くすことができない。exponent_ を4のままにすれば物理どおりの
	* 減光になり、上げ下げするのは純粋なアートディレクション。
	*
	* 露出とトーンカーブより【前】に適用する。ビネットは「そもそもセンサーへ
	* 届かなかった光」であって、露出済みの結果へ後から塗る暗がりではない
	* ため、露出される対象そのものに含まれていなければならない。
	*/
	struct VignetteSettings
	{
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = false;

		/// [EN] Exponent applied to the falloff curve. 1 = the physical
		///      cosine-fourth curve as-is, 0 = no vignette, above 1 pushes
		///      the corners darker than the physical curve.
		/// [JP] 減光カーブに掛かる指数。1で物理どおりのコサイン4乗カーブ
		///      そのまま、0で無効、1を超えると物理カーブより四隅が暗くなる。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("強度", 0.0f, 8.0f)
		Float intensity_ = 0.5f;

		/// [EN] 4 is the physical cosine-fourth law (see the struct comment).
		///      Anything else is art direction.
		/// [JP] 4 が物理どおりのコサイン4乗則(struct のコメント参照)。
		///      それ以外の値はアートディレクション。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("減光指数", 1.0f, 8.0f)
		Float exponent_ = 4.0f;

		/// [EN] What the corners fall off toward. Black is the physical
		///      answer (absence of light); anything else is a look.
		/// [JP] 四隅が何色へ向かって落ちるか。物理的な答えは黒(光が無い
		///      ということ)で、それ以外は演出。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_FIELD_EX("色")
		Color color_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	};

	/**
	* [EN]
	* Bloom settings: the broad glow that bright parts of the scene bleed into
	* their surroundings. Implemented (KawaseBloomCS.hlsl) as a progressive
	* downsample chain followed by a progressive additive upsample chain over
	* 6 levels starting at half the native resolution - the Kawase-lineage
	* structure, using the tap patterns from Jimenez's Call of Duty: Advanced
	* Warfare post-processing talk (13-tap downsample, 3x3 tent upsample). The
	* first downsample additionally applies a Karis average, which is what
	* keeps a single blown-out subpixel (easy to hit with this engine's
	* ray-traced HDR values) from turning into a large flickering block as it
	* propagates up the chain. threshold_/softKnee_ select what bleeds:
	* softKnee_ widens the transition around threshold_ so pixels hovering at
	* the cutoff fade in smoothly instead of popping. Added into the HDR color
	* before exposure in ToneMappingCS.hlsl, the same place LensFlareSettings'
	* contribution goes - bloom is light, so it gets exposed like light.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ブルーム設定: シーンの明るい部分が周囲へにじみ出る広いグロー。
	* 実装(KawaseBloomCS.hlsl)はネイティブ解像度の1/2から始まる6レベルの
	* 段階的ダウンサンプルチェーンと、それに続く段階的な加算アップサンプル
	* チェーン — 構造は Kawase 系列で、タップパターンは Jimenez の
	* Call of Duty: Advanced Warfare のポストプロセス講演のもの
	* (13タップダウンサンプル、3x3テントアップサンプル)を使う。初段の
	* ダウンサンプルにはさらに Karis 平均を掛ける。これは1画素だけ飛び抜けて
	* 明るい点(このエンジンのレイトレHDR値では簡単に起きる)が、チェーンを
	* 昇るにつれて巨大なちらつくブロックに育つのを防ぐためのもの。
	* threshold_/softKnee_ がにじむ範囲を決める: softKnee_ は threshold_
	* 周辺の遷移を広げ、しきい値ぎりぎりの明るさの画素が突然出入りせず
	* 滑らかにフェードするようにする。LensFlareSettings の寄与と同じく
	* ToneMappingCS.hlsl で露出適用前のHDRカラーへ加算する — ブルームも
	* 光なので、光として露出される。
	*/
	struct BloomSettings
	{
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = false;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("しきい値", 0.0f, 10.0f)
		Float threshold_ = 1.0f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("ソフトニー", 0.0f, 1.0f)
		Float softKnee_ = 0.5f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("強度", 0.0f, 1.0f)
		Float intensity_ = 0.08f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("フィルタ半径", 0.001f, 0.02f)
		Float filterRadius_ = 0.005f;
	};

	/**
	* [EN]
	* Anamorphic flare settings: the long horizontal streak a cine anamorphic
	* lens throws off bright points. Separate from LensFlareSettings because
	* the cause is different - that one is aperture diffraction plus
	* inter-element reflections, this one is the cylindrical squeeze element.
	*
	* The streak is horizontal for a non-obvious reason. An anamorphic lens
	* SQUEEZES the image 2:1 horizontally while shooting; the internal
	* reflection happens inside that squeezed space as an ordinary round
	* flare; then projection stretches the image back out, and the round
	* flare becomes a horizontal streak. So AnamorphicFlareCS.hlsl blurs in a
	* horizontally-squeezed buffer and samples it back with normal UVs - the
	* stretch falls out of the sampling for free, no directional blur needed
	* for it. A 2:1 squeeze alone only gives a 2:1 ellipse though, so the blur
	* inside that space is additionally horizontal and streakLength_ long:
	* that part is art direction, not physics, and it is not energy
	* conserving (every shipped implementation of this effect does the same).
	*
	* tint_ is NOT a physical constant. The iconic blue comes from
	* blue-tinted cylindrical elements popularised by Ira Tiffen's filters;
	* other lenses flare gold or amber (Cooke), so the colour is a property
	* of the particular lens being imitated and belongs in the settings.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アナモルフィックフレア設定: シネマ用アナモルフィックレンズが明るい点に
	* 対して出す、横方向の長い筋。LensFlareSettings と分けているのは原因が
	* 別だから — あちらは絞りの回折と素子間反射、こちらはシリンドリカル
	* (円柱)圧縮素子によるもの。
	*
	* 筋が横向きになる理由は直感に反する。アナモルフィックレンズは撮影時に
	* 像を水平方向へ【2:1に圧縮】する。レンズ内部反射はその圧縮された空間の
	* 中で普通の丸いフレアとして起き、上映時に水平へ引き伸ばして戻すことで、
	* 丸かったフレアが横長の筋になる。したがって AnamorphicFlareCS.hlsl は
	* 「横に潰したバッファでブラーし、通常のUVでサンプルして戻す」という
	* 順序を取る — 引き伸ばしはサンプル時に勝手に起きるので、そのための
	* 方向ブラーは要らない。ただし 2:1 の圧縮だけでは 2:1 の楕円にしか
	* ならないため、その空間の中でさらに横方向へ streakLength_ ぶん
	* ブラーして長さを稼ぐ。こちらは物理ではなくアートディレクションで、
	* エネルギー保存もしていない(このエフェクトの実装は軒並みそうしている)。
	*
	* tint_ は【物理定数ではない】。象徴的な青は Ira Tiffen のフィルタが
	* 広めた青く着色したシリンドリカル素子に由来するもので、レンズによっては
	* 金色/琥珀色に出る(Cooke 系)。つまり色は「模倣したいレンズ個体の
	* 特性」であって、設定として持つのが正しい。
	*/
	struct AnamorphicFlareSettings
	{
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = false;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("しきい値", 0.0f, 20.0f)
		Float threshold_ = 4.0f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("強度", 0.0f, 5.0f)
		Float intensity_ = 0.6f;

		/// [EN] Tap spacing scale inside the squeezed buffer - how far the
		///      streak reaches horizontally.
		/// [JP] 圧縮バッファ内でのタップ間隔スケール。筋が横方向へどこまで
		///      伸びるかを決める。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("筋の長さ", 0.0f, 1.0f)
		Float streakLength_ = 0.5f;

		/// [EN] Kawase's light-streak attenuation `a`: the per-texel decay
		///      along the streak, so a tap `d` texels out is weighted a^d.
		/// [JP] Kawase のライトストリークの減衰係数 a。筋に沿った1テクセル
		///      あたりの減衰率で、d テクセル先のタップの重みが a^d になる。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("減衰", 0.80f, 0.99f)
		Float attenuation_ = 0.95f;

		/// [EN] Streak colour. A lens characteristic, not physics - see the
		///      struct comment. Default is the familiar cine blue.
		/// [JP] 筋の色。物理ではなくレンズ個体の特性(struct のコメント参照)。
		///      既定値は見慣れたシネマ調の青。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_FIELD_EX("色")
		Color tint_ = { 0.35f, 0.6f, 1.0f, 1.0f };
	};

	/**
	* [EN]
	* Lens flare (LensFlareCS.hlsl), two effects sharing one settings block
	* because they come from the same lens:
	*
	* - The aperture diffraction spikes ("starburst"). Multi-pass Kawase
	*   directional streaks whose spike count is NOT authored here - it
	*   follows from the iris, so it is derived from BokehSettings::
	*   bladeCount_ (the same physical aperture that gives the bokeh its
	*   shape). n blades give n spikes when n is even and 2n when odd,
	*   because each blade edge diffracts perpendicular to itself and on an
	*   even-bladed iris the opposing edges are parallel so their spikes
	*   coincide. streakAttenuation_ is the per-texel decay along a spike and
	*   streakLength_ scales the tap spacing; chromaticAberration_ separates
	*   R/G/B along the spike, which is the right direction physically since
	*   the diffraction angle grows with wavelength.
	* - The ghost chain and halo, following John Chapman's pseudo lens flare:
	*   bright-passed copies of the source strung through screen center to
	*   the opposite side (ghostCount_/ghostDispersal_/ghostIntensity_) plus
	*   a ring at haloWidth_. These are inter-element REFLECTIONS, not
	*   diffraction, which is why they move with the light's position while
	*   the spikes stay locked to the screen.
	*
	* Runs at a quarter of the native resolution and is added into the HDR
	* color (before exposure) in ToneMappingCS.hlsl.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レンズフレア(LensFlareCS.hlsl)。同じレンズが作るものなので、2つの
	* 効果が1つの設定ブロックを共有している:
	*
	* - 絞りの回折スパイク(スターバースト)。多段階Kawase方向ストリークで
	*   作るが、棘の本数はここで指定しない — 絞りから決まるものなので
	*   BokehSettings::bladeCount_(ボケの形を決めるのと同じ物理的な絞り)
	*   から導出する。羽根n枚で、偶数ならn本・奇数なら2n本。各羽根の
	*   エッジがそれ自身に垂直な方向へ回折し、偶数枚だと向かい合うエッジが
	*   平行で棘が重なるため。streakAttenuation_ は棘に沿った1テクセル
	*   あたりの減衰、streakLength_ はタップ間隔のスケール。
	*   chromaticAberration_ は棘に沿ってR/G/Bを分離する — 回折角は波長と
	*   共に大きくなるので、方向としては物理どおり。
	* - ゴーストチェーンとハロー。John Chapman の pseudo lens flare に
	*   従う: 画面中心を通って反対側まで連なる光源のブライトパス済みコピー
	*   (ghostCount_/ghostDispersal_/ghostIntensity_)と、haloWidth_ の輪。
	*   これらは回折ではなくレンズ素子間の【反射】で、だから光源の位置に
	*   応じて動く(棘が画面に固定なのと対照的)。
	*
	* ネイティブ解像度の1/4で走り、ToneMappingCS.hlsl で(露出適用前の)
	* HDRカラーへ加算合成する。
	*/
	struct LensFlareSettings
	{
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = false;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("しきい値", 0.0f, 20.0f)
		Float threshold_ = 4.0f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("全体強度", 0.0f, 5.0f)
		Float intensity_ = 0.5f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("スパイク長", 0.0f, 1.0f)
		Float streakLength_ = 0.25f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("減衰", 0.80f, 0.99f)
		Float streakAttenuation_ = 0.93f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("色収差", 0.0f, 0.02f)
		Float chromaticAberration_ = 0.004f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("回転", -3.14159265f, 3.14159265f)
		Float angleOffset_ = 0.0f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("ゴースト数", 1, 8)
		Uint32 ghostCount_ = 4;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("ゴースト間隔", 0.0f, 1.0f)
		Float ghostDispersal_ = 0.3f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("ゴースト強度", 0.0f, 2.0f)
		Float ghostIntensity_ = 0.3f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("ハロー半径", 0.0f, 1.0f)
		Float haloWidth_ = 0.45f;

		/// [EN] Per-spike randomisation of brightness and length, keyed off
		///      the spike index so it is stable frame to frame. NOT physics:
		///      an ideal iris is perfectly symmetric and produces identical
		///      spikes. Real lenses do not, because the blades are neither
		///      identical nor perfectly aligned, and that asymmetry is most
		///      of what separates a photographed starburst from a CG
		///      asterisk. 0 disables it and gives the ideal symmetric star.
		/// [JP] 棘ごとの明るさ・長さのばらつき。棘の番号から決めるので
		///      フレーム間で安定する。【物理ではない】: 理想的な絞りは
		///      完全に対称で、棘は全て同一になる。実際のレンズがそうならない
		///      のは羽根が同一でも完全な位置合わせでもないからで、その
		///      非対称性こそが実写のスターバーストとCGのアスタリスクを
		///      分けている大部分。0で無効になり、理想的な対称の星になる。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("棘のばらつき", 0.0f, 1.0f)
		Float spikeVariation_ = 0.35f;
	};

	/**
	* [EN]
	* Exposure settings, its own reflected struct — see ToneMappingSettings
	* below for why each effect gets one rather than living as flat fields on
	* PostProcess. compensation_ is the manual EV knob and always applies (it
	* is the exposure system's fallback when the histogram-based auto exposure
	* below is off, same relationship as Unreal's "Exposure Compensation"
	* sitting in the same category as auto-exposure metering); the histogram/
	* adaptation fields only matter when enabled_ is on, so only those carry a
	* condition.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 露出設定。専用のリフレクション対象構造体にする理由は下の
	* ToneMappingSettings 参照。compensation_ は手動 EV のつまみで常に効く
	* (下のヒストグラムベース自動露出が無効なときの露出系のフォールバックが
	* これ — Unreal の「露出補正」が自動露出の計測方式と同じカテゴリに
	* 並んでいるのと同じ関係)。ヒストグラム/順応系のフィールドは enabled_ が
	* 有効な時だけ意味を持つので、そちらにだけ Condition を付けている。
	*/
	struct ExposureSettings
	{
		/// [EN] Exposure compensation in stops, ALWAYS applied (not gated by
		///      enabled_ below) on top of the automatic value (0 when auto
		///      exposure is off, so this is the only exposure control in that
		///      case). The scene is rendered in radiance units where a sunlit
		///      white Lambertian surface lands at about 0.318 (the BRDF
		///      carries the 1/PI), so roughly +1.65 EV is needed to reach
		///      display white when auto exposure is off — that is why the
		///      default is not 0.
		/// [JP] 露出補正(EV/段)。下の enabled_ に関わらず【常に】効く、自動値
		///      への加算値(自動露出が無効なら自動値は0なので、その場合は
		///      これが唯一の露出調整になる)。シーンは「日向の白いランバート面
		///      ≒ 0.318」という放射輝度の単位で描かれている(BRDF が 1/PI を
		///      含むため)ので、自動露出が無効な状態で表示上の白へ持ち上げる
		///      にはおよそ +1.65EV 要る。既定値が 0 でないのはそのため。
		SC_REFLECTION_CLAMPED_EX("露出補正(EV)", -8.0f, 8.0f)
		Float compensation_ = 1.65f;

		SC_REFLECTION_FIELD_EX("自動露出(ヒストグラム)を使う")
		Bool enabled_ = false;

		/// [EN] Log2 luminance range the histogram covers. Pixels outside this
		///      range clamp to the nearest edge bin. Widen it if very dark
		///      interiors or very bright skies never seem to adapt.
		/// [JP] ヒストグラムが対象とする log2 輝度の範囲。範囲外のピクセルは
		///      最寄りの端のビンへクランプされる。暗い室内や明るい空へ順応が
		///      効かない場合は広げる。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("最小log輝度", -16.0f, 0.0f)
		Float minLogLuminance_ = -10.0f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("最大log輝度", 0.0f, 16.0f)
		Float maxLogLuminance_ = 4.0f;

		/// [EN] Target average scene luminance (the "18% grey card" concept from
		///      photography, expressed in this engine's own radiance units, not
		///      the traditional 0.18 — see compensation_'s comment on this
		///      engine's ~0.318 white reference).
		/// [JP] シーン平均輝度の目標値(写真の「18%グレーカード」の考え方を、この
		///      エンジンの放射輝度単位で表したもの — 伝統的な 0.18 ではない点に
		///      注意。compensation_ のコメントにある通り、このエンジンの白基準は
		///      約 0.318)。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("目標輝度(キー値)", 0.001f, 1.0f)
		Float keyValue_ = 0.18f;

		/// [EN] Adaptation speed (in 1/seconds, larger = faster). Photography/
		///      vision terminology: 明順応 (light adaptation) is adapting TO a
		///      brighter scene - fast, the iris stops down quickly; 暗順応 (dark
		///      adaptation) is adapting TO a darker scene - slow, rod recovery
		///      takes time. Two knobs instead of one so that asymmetry is
		///      tunable rather than baked in.
		/// [JP] 順応速度(1/秒、大きいほど速い)。明順応=より明るいシーンへの
		///      順応(速い、虹彩がすぐ絞る)、暗順応=より暗いシーンへの順応
		///      (遅い、桿体の回復に時間がかかる)。1つの速度に固定せず2つの
		///      つまみにして、この非対称性を調整可能にしている。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("明順応速度", 0.01f, 20.0f)
		Float adaptSpeedToBright_ = 3.0f;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("暗順応速度", 0.01f, 20.0f)
		Float adaptSpeedToDark_ = 1.0f;
	};

	/**
	* [EN]
	* One tonal range's worth of colour grading controls, in the order they
	* are applied: white balance, then saturation, contrast, gamma, gain,
	* offset. That order is not arbitrary - the five wheels are Unreal's
	* order and each stage operates on the previous stage's output, and white
	* balance goes ahead of all of them because it is a capture-side
	* correction rather than a creative one.
	*
	* Contrast pivots around 0.18, scene-referred middle grey. That pivot is
	* the reason grading has to run in scene-referred linear space after
	* exposure and before the tone curve: 0.18 only means "middle grey" once
	* exposure has placed the scene there, and stops meaning anything at all
	* after the tone curve has compressed the range.
	*
	* temperature_ is the only control that touches colour; the other four
	* are plain scalars applied uniformly across R/G/B. Neutral is 1 for
	* saturation/contrast/gamma/gain and 0 for offset/temperature.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 1つの階調域ぶんのカラーグレーディング操作。適用される順に、
	* ホワイトバランス → 彩度 → コントラスト → ガンマ → ゲイン →
	* オフセット。この順序は恣意的ではなく、ホイール5つは Unreal の
	* パネルと同じ順で、各段が前段の出力に対して働くため意味を持つ。
	* ホワイトバランスが全ての前に来るのは、創作的な操作ではなく
	* 撮影側の補正だから。
	*
	* コントラストは 0.18(シーン参照の中間グレー)を軸に回す。この軸こそが、
	* グレーディングを「露出の後、トーンカーブの前」のシーン参照リニア空間で
	* 走らせなければならない理由: 0.18 が「中間グレー」を意味するのは露出が
	* シーンをそこへ置いた後だけで、トーンカーブがレンジを圧縮した後では
	* 何の意味も持たなくなる。
	*
	* 色に触れる操作は temperature_ だけで、残り4つはR/G/Bへ一律に掛かる
	* 単純なスカラー。中立値は彩度/コントラスト/ガンマ/ゲインが1、
	* オフセットと色温度が0。
	*/
	struct ColorGradingRangeSettings
	{
		/// [EN] White balance for this range, applied first - it is a
		///      capture-side correction, so it belongs before the creative
		///      wheels rather than after them. Negative is cooler (blue),
		///      positive warmer (amber). This is a real chromatic adaptation
		///      (CIE xy white point -> LMS cone space -> von Kries scale),
		///      not an RGB tint, which is why it shifts the whole image's
		///      white point instead of just staining it.
		///
		///      Unreal keeps white balance global; per-range is strictly more
		///      capable - cooling shadows while warming highlights is a
		///      standard grade and needs the two independently. Note this is
		///      only half of white balance: its partner is TINT
		///      (green<->magenta), which is not exposed here.
		/// [JP] この階調域のホワイトバランス。撮影側の補正なので、創作的な
		///      ホイール群より【前】に適用する。負で寒色(青)、正で暖色(琥珀)。
		///      RGBの色被せではなく本物の色順応(CIE xy の白色点 → LMS 錐体
		///      空間 → von Kries スケール)なので、色を塗るのではなく画像
		///      全体の白色点そのものが動く。
		///
		///      Unreal はホワイトバランスを全体設定として持つが、階調域ごとに
		///      持つ方が純粋に強い — シャドウを寒色に、ハイライトを暖色に、は
		///      定番のグレーディングで、独立していないとできない。なお
		///      これはホワイトバランスの半分で、相方の【ティント】
		///      (緑↔マゼンタ)はここでは公開していない。
		SC_REFLECTION_CLAMPED_EX("色温度", -1.0f, 1.0f)
		Float temperature_ = 0.0f;

		/// [EN] These five are scalars, applied uniformly across R/G/B,
		///      rather than per-channel wheels. The colour axis is already
		///      covered by temperature_ above as a proper chromatic
		///      adaptation, so these five stay pure brightness/contrast
		///      controls.
		/// [JP] この5つはチャンネル別のホイールではなく、R/G/Bへ一律に
		///      掛かるスカラー。色方向は上の temperature_ が本物の色順応
		///      として担当しているので、この5つは純粋な明るさ・
		///      コントラスト系の操作にとどめてある。
		SC_REFLECTION_CLAMPED_EX("彩度", 0.0f, 4.0f)
		Float saturation_ = 1.0f;

		SC_REFLECTION_CLAMPED_EX("コントラスト", 0.1f, 4.0f)
		Float contrast_ = 1.0f;

		/// [EN] Lower bound is 0.1, not 0. Gamma is inverted before being used
		///      as an exponent, so letting it approach 0 sends the exponent
		///      toward infinity and crushes the whole frame to black.
		/// [JP] 下限が0ではなく0.1なのは、ガンマが指数として使われる前に
		///      逆数を取られるため。0へ近づけると指数が発散し、画面全体が
		///      黒へ潰れる。
		SC_REFLECTION_CLAMPED_EX("ガンマ", 0.1f, 4.0f)
		Float gamma_ = 1.0f;

		SC_REFLECTION_CLAMPED_EX("ゲイン", 0.0f, 4.0f)
		Float gain_ = 1.0f;

		SC_REFLECTION_CLAMPED_EX("オフセット", -1.0f, 1.0f)
		Float offset_ = 0.0f;
	};

	/**
	* [EN]
	* Unreal-style colour grading: four tonal ranges (global, shadows,
	* midtones, highlights), each with its own saturation/contrast/gamma/
	* gain/offset wheels. The per-range values are combined with the global
	* ones rather than replacing them, so global stays a master move and the
	* ranges are relative adjustments on top of it.
	*
	* Which range a pixel belongs to is decided by its luminance, with two
	* smooth crossovers rather than hard cuts: everything below shadowsMax_
	* is shadow, everything above highlightsMin_ is highlight, and the
	* midtone weight is whatever is left over. Smooth crossovers matter -
	* hard thresholds put a visible contour line through any smooth gradient
	* the moment the two ranges are graded differently.
	*
	* Runs in scene-referred linear space after exposure and before the tone
	* curve, which is where Unreal puts it too. See
	* ColorGradingRangeSettings' comment for why the 0.18 contrast pivot
	* forces that position.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Unreal 方式のカラーグレーディング: 4つの階調域(全体・シャドウ・中間調・
	* ハイライト)それぞれに彩度/コントラスト/ガンマ/ゲイン/オフセットの
	* ホイールを持つ。階調域ごとの値は全体の値を置き換えるのではなく
	* 掛け合わせるので、全体はマスターの動き、各域はその上に乗る相対的な
	* 調整という関係になる。
	*
	* どの階調域に属するかは輝度で決まり、境界は硬い切り替えではなく2つの
	* なだらかなクロスオーバーになっている: shadowsMax_ より下が全て
	* シャドウ、highlightsMin_ より上が全てハイライト、残りが中間調の重み。
	* なだらかであることは重要で、硬いしきい値にすると2つの域を別々に
	* グレーディングした瞬間、滑らかなグラデーションに等高線が見えてしまう。
	*
	* 露出の後・トーンカーブの前、シーン参照リニア空間で走る。Unreal も
	* 同じ位置に置いている。0.18 のコントラスト軸がなぜその位置を強制するかは
	* ColorGradingRangeSettings のコメント参照。
	*/
	struct ColorGradingSettings
	{
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = false;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_FIELD_EX("全体")
		ColorGradingRangeSettings global_;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_FIELD_EX("シャドウ")
		ColorGradingRangeSettings shadows_;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_FIELD_EX("中間調")
		ColorGradingRangeSettings midtones_;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_FIELD_EX("ハイライト")
		ColorGradingRangeSettings highlights_;

		/// [EN] Luminance below which a pixel is fully shadow.
		/// [JP] これを下回る輝度の画素は完全にシャドウ扱いになる。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("シャドウ上限", 0.0f, 1.0f)
		Float shadowsMax_ = 0.09f;

		/// [EN] Luminance above which a pixel is fully highlight.
		/// [JP] これを上回る輝度の画素は完全にハイライト扱いになる。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("ハイライト下限", 0.0f, 2.0f)
		Float highlightsMin_ = 0.5f;
	};

	/**
	* [EN]
	* Tone mapping settings, its own reflected struct rather than a handful of
	* flat fields on PostProcess directly — each post effect gets one of these
	* (BloomSettings, VignetteSettings, ...) so the inspector groups them and
	* PostProcess itself stays a plain list of "one field per effect" instead of
	* growing into a soup of unrelated bools and floats.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* トーンマップ設定。PostProcess に直接フラットなフィールドを並べるのではなく、
	* 独立したリフレクション対象の構造体にする — 各ポストエフェクトはこの形を1つ
	* 持ち(BloomSettings、VignetteSettings、...)、インスペクタ上でグループ化され、
	* PostProcess 自体は「エフェクトごとに1フィールド」の単純な並びのまま、無関係な
	* bool/float の寄せ集めに膨らまずに済む。
	*/
	struct ToneMappingSettings
	{
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = true;

		enum class ToneCurve
		{
			None,      // クランプのみ、カーブ無し
			Reinhard,
			AcesFilmic,
			PbrNeutral // Khronos PBR Neutral
		};

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_FIELD_EX("方式")
		ToneCurve mode_ = ToneCurve::AcesFilmic;
	};

	/**
	* [EN]
	* Sharpness (unsharp mask) settings for the final display pass
	* (SharpnessCS.hlsl). Runs after ToneMappingSettings, sampling a 5-tap
	* cross around each pixel of the tone-mapped/sRGB-encoded output and
	* pushing each pixel away from its neighborhood average by amount_ - the
	* classic unsharp-mask edge enhancement, applied in the same LDR space
	* most engines apply CAS/sharpen filters in.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 最終表示パス向けのシャープネス(アンシャープマスク)設定
	* (SharpnessCS.hlsl)。ToneMappingSettings の後に走り、トーンマップ/
	* sRGBエンコード済み出力の各ピクセル周囲を5タップの十字でサンプルし、
	* 近傍平均から amount_ ぶん引き離す — 典型的なアンシャープマスクによる
	* 輪郭強調。多くのエンジンがCAS等のシャープフィルタを適用するのと同じ
	* LDR空間で適用する。
	*/
	struct SharpnessSettings
	{
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = false;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("強さ", 0.0f, 2.0f)
		Float amount_ = 0.5f;
	};

	/**
	* [EN]
	* Film grain settings. The non-obvious part is luminanceResponse_: real
	* film grain is strongest in the MIDTONES, not in the shadows and not in
	* the highlights. In shadows few silver halide crystals were exposed, so
	* there is little to see; in highlights so many were exposed that they
	* pack together into continuous tone and individual grains stop being
	* distinguishable. Only the midtones sit at the density where grains are
	* both numerous and separable. Uniform noise across the whole tonal range
	* is the usual giveaway that grain was faked, which is why the response
	* curve here peaks at mid grey and falls off toward both ends.
	*
	* size_ matters for the same reason: film grain is a clump of crystals,
	* not a pixel. Per-pixel noise reads as digital sensor noise or video
	* static; sampling the noise field at a coarser scale gives grains with
	* actual size.
	*
	* Applied after tone mapping, unlike the lens-stage effects. Grain is a
	* property of the developed image - the density variation in the emulsion
	* - so the tonal position that drives the response curve above only means
	* anything once the tone curve has been applied.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* フィルムグレイン設定。直感に反するのは luminanceResponse_ の部分:
	* 実際のフィルムグレインが最も強く出るのは【中間調】で、シャドウでも
	* ハイライトでもない。シャドウは露光したハロゲン化銀結晶が少ないので
	* 見えるものが少なく、ハイライトは露光した結晶が多すぎて互いに詰まり
	* 連続した階調に溶けるため個々の粒が判別できなくなる。粒が「多く、かつ
	* 分離して見える」濃度に居るのは中間調だけ。階調全域に一様なノイズを
	* 乗せるのが「グレインが偽物」と分かる典型的な兆候で、ここの応答カーブが
	* 中間グレーで最大になり両端へ向かって落ちるのはそのため。
	*
	* size_ が重要なのも同じ理由。フィルムグレインは結晶の塊であって画素では
	* ない。画素単位のノイズはデジタルのセンサーノイズかテレビの砂嵐に見える。
	* ノイズ場を粗いスケールでサンプルすることで、粒に実際の大きさが出る。
	*
	* レンズ段のエフェクトと違い、トーンマップの【後】に適用する。グレインは
	* 現像された画像の性質 — 乳剤の濃度ムラ — なので、上の応答カーブを
	* 駆動する階調上の位置は、トーンカーブを通した後でなければ意味を持たない。
	*/
	struct FilmGrainSettings
	{
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = false;

		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("強度", 0.0f, 1.0f)
		Float intensity_ = 0.15f;

		/// [EN] Grain size in pixels. 1 gives per-pixel noise, which reads as
		///      digital noise rather than film.
		/// [JP] 粒の大きさ(画素単位)。1にすると画素単位のノイズになり、
		///      フィルムではなくデジタルノイズに見える。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("粒の大きさ", 1.0f, 8.0f)
		Float size_ = 2.0f;

		/// [EN] How strongly grain follows the midtone-peaked film response.
		///      0 applies it uniformly across the tonal range, which is what
		///      fake grain looks like.
		/// [JP] 中間調で最大になるフィルムの応答にどれだけ従うか。0にすると
		///      階調全域へ一様に乗り、いかにも作り物のグレインになる。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_CLAMPED_EX("階調応答", 0.0f, 1.0f)
		Float luminanceResponse_ = 1.0f;

		/// [EN] Colour film records grain per emulsion layer, so the grain is
		///      coloured. Monochrome grain is the cheaper approximation.
		/// [JP] カラーフィルムは乳剤層ごとに粒を持つので、グレインにも色が
		///      付く。モノクロのグレインはそれを簡略化したもの。
		SC_REFLECTION_FIELD_CONDITION(enabled_)
		SC_REFLECTION_FIELD_EX("色付き")
		Bool colored_ = true;
	};

	/**
	* [EN]
	* Post-process settings, authored as a component on an entity (the same
	* shape as Unreal's PostProcessVolume or Unity's Post Process Volume). The
	* renderer gathers it with Query<Read<Active>, Read<PostProcess>> and runs
	* the effect chain in PostProcess/PostEffect/ with these values; when no
	* entity carries one, the renderer falls back to this struct's defaults so
	* a scene without a post-process entity still tonemaps.
	*
	* Each effect gets its own reflected Settings struct (see
	* ToneMappingSettings/ExposureSettings above) plus one field here and its
	* own class under PostEffect/, so this component stays the single
	* authoring surface without turning into a flat pile of unrelated fields.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ポストプロセス設定。エンティティに付けるコンポーネントとして持たせる
	* (Unreal の PostProcessVolume / Unity の Post Process Volume と同じ形)。
	* レンダラーが Query<Read<Active>, Read<PostProcess>> で拾い、
	* PostProcess/PostEffect/ のエフェクト列をこの値で走らせる。どのエンティティ
	* も持っていない場合はこの構造体の既定値へフォールバックするので、
	* ポストプロセス用エンティティが無いシーンでもトーンマップは掛かる。
	*
	* 各エフェクトは上の ToneMappingSettings/ExposureSettings のような専用の
	* リフレクション対象構造体を持ち、ここにフィールドを1つ足し、
	* PostEffect/ にクラスを足す — 無関係なフィールドの寄せ集めにしない。
	*/
	struct PostProcess
	{
		/// [EN] Fields are ordered to match the order the passes actually run
		///      in PostProcessRenderer::Dispatch, so reading the inspector
		///      top to bottom is reading the frame in the order it is built.
		/// [JP] フィールドの並びは PostProcessRenderer::Dispatch で実際に
		///      パスが走る順に合わせてある。インスペクタを上から下へ読むと、
		///      フレームが組み立てられる順に読めることになる。

		SC_REFLECTION_FIELD_EX("被写界深度")
		DepthOfFieldSettings depthOfField_;

		SC_REFLECTION_FIELD_EX("ボケ")
		BokehSettings bokeh_;

		SC_REFLECTION_FIELD_EX("レンズ歪曲")
		LensDistortionSettings lensDistortion_;

		SC_REFLECTION_FIELD_EX("色収差")
		ChromaticAberrationSettings chromaticAberration_;

		SC_REFLECTION_FIELD_EX("ビネット")
		VignetteSettings vignette_;

		SC_REFLECTION_FIELD_EX("ブルーム")
		BloomSettings bloom_;

		SC_REFLECTION_FIELD_EX("アナモルフィックフレア")
		AnamorphicFlareSettings anamorphicFlare_;

		SC_REFLECTION_FIELD_EX("レンズフレア")
		LensFlareSettings lensFlare_;

		SC_REFLECTION_FIELD_EX("露出")
		ExposureSettings exposure_;

		SC_REFLECTION_FIELD_EX("カラーグレーディング")
		ColorGradingSettings colorGrading_;

		SC_REFLECTION_FIELD_EX("トーンマップ")
		ToneMappingSettings toneMapping_;

		SC_REFLECTION_FIELD_EX("シャープネス")
		SharpnessSettings sharpness_;

		SC_REFLECTION_FIELD_EX("フィルムグレイン")
		FilmGrainSettings filmGrain_;
	};
	REGISTER_COMPONENT(PostProcess, "PostProcess");
}

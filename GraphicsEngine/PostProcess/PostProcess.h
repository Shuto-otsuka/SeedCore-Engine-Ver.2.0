#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
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
	* Tone mapping settings, its own reflected struct rather than a handful of
	* flat fields on PostProcess directly — each post effect gets one of these
	* (BloomSettings, VignetteSettings, ...) so the inspector groups them and
	* PostProcess itself stays a plain list of "one field per effect" instead of
	* growing into a soup of unrelated bools and floats.
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

		/// [EN] Named ToneCurve, not Mode - EnumRegistry keys enums by their
		///      bare type name only (FlatMap<String, DynamicArray<EnumEntry>>,
		///      no owning-struct qualification), so "Mode" collided with
		///      CameraSystem::Mode ({Free, User}) at registration: whichever
		///      one's static initializer ran last silently won the shared
		///      slot, and the other's inspector dropdown showed the wrong
		///      entries. The real fix is qualifying EnumRegistry's key by
		///      owner type; short of that, just avoid common generic names.
		/// [JP] Mode ではなく ToneCurve という名前にしている理由: EnumRegistry は
		///      列挙型を裸の型名だけでキーする(FlatMap<String,
		///      DynamicArray<EnumEntry>>、所有側構造体による修飾は無い)ため、
		///      "Mode" が CameraSystem::Mode({Free, User})と登録時に衝突して
		///      いた — どちらの静的初期化子が最後に走るかで共有スロットを
		///      黙って奪い合い、負けた側のインスペクタのドロップダウンには
		///      相手の項目が出ていた。本質的な直し方は EnumRegistry のキーを
		///      所有側の型で修飾することだが、それをしない限りは、ありふれた
		///      汎用名を避けるのが現実的な対処。
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
	* enabled_ also being on - PostProcessRenderer::Dispatch enforces that at
	* runtime (Reflection.py's SC_REFLECTION_FIELD_CONDITION can only
	* reference a field on this same struct, not a sibling struct's field, so
	* the inspector cannot nest this checkbox under DepthOfFieldSettings.
	* enabled_).
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
	* PostProcessRenderer::Dispatch が担保する(Reflection.py の
	* SC_REFLECTION_FIELD_CONDITION は同じ struct 内のフィールドしか参照
	* できず、別の struct のフィールドは参照できないため、インスペクタ上で
	* このチェックボックスを DepthOfFieldSettings.enabled_ の下にネストする
	* ことはできない)。
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
	* Post-process settings, authored as a component on an entity (the same
	* shape as Unreal's PostProcessVolume or Unity's Post Process Volume). The
	* renderer gathers it with Query<Read<Active>, Read<PostProcess>> and runs
	* the effect chain in PostProcess/PostEffect/ with these values; when no
	* entity carries one, the renderer falls back to this struct's defaults so
	* a scene without a post-process entity still tonemaps.
	*
	* Only exposure, bloom, lens flare, depth of field, bokeh, tone mapping,
	* and sharpness are implemented so far - the other files under PostEffect/
	* are empty stubs. Each new effect gets its own reflected Settings struct
	* (see ToneMappingSettings/ExposureSettings above) plus one field here
	* and its own class under PostEffect/, so this component stays the
	* single authoring surface without turning into a flat pile of unrelated
	* fields.
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
	* 現状は露出・ブルーム・レンズフレア・被写界深度・ボケ・トーンマップ・
	* シャープネスのみ実装 — PostEffect/ の他のファイルは空スタブ。エフェクトを増やす
	* ときは上の ToneMappingSettings/ExposureSettings のような専用の
	* リフレクション対象構造体を作り、ここにフィールドを1つ足し、
	* PostEffect/ にクラスを足す — 無関係なフィールドの寄せ集めにしない。
	*/
	struct PostProcess
	{
		SC_REFLECTION_FIELD_EX("露出")
		ExposureSettings exposure_;

		SC_REFLECTION_FIELD_EX("ブルーム")
		BloomSettings bloom_;

		SC_REFLECTION_FIELD_EX("レンズフレア")
		LensFlareSettings lensFlare_;

		SC_REFLECTION_FIELD_EX("被写界深度")
		DepthOfFieldSettings depthOfField_;

		SC_REFLECTION_FIELD_EX("ボケ")
		BokehSettings bokeh_;

		SC_REFLECTION_FIELD_EX("トーンマップ")
		ToneMappingSettings toneMapping_;

		SC_REFLECTION_FIELD_EX("シャープネス")
		SharpnessSettings sharpness_;
	};
	REGISTER_COMPONENT(PostProcess, "PostProcess");
}

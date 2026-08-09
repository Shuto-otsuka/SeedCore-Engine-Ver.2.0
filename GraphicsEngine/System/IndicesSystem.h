#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>

namespace SeedCore
{
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
		Uint lensFlarePadding_[3] = { 0, 0, 0 };
	};
	static_assert(sizeof(LensFlareIndices) % 16 == 0, "LensFlareIndices が 16 バイト行の倍数ではありません");

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
		Uint postProcessPadding_[2] = { 0, 0 };

		ExposureIndices exposure_;
		ToneMappingIndices toneMapping_;
		LensFlareIndices lensFlare_;
		DepthOfFieldIndices depthOfField_;
		BokehIndices bokeh_;
	};
	static_assert(sizeof(PostProcessIndices) == 176, "PostProcessIndices が Shader/Constants.hlsli と一致していません");

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
	static_assert(sizeof(ConstantIndices) == 20 * 16, "ConstantIndices が Shader/Constants.hlsli と一致していません");

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
#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/SerializeFallback.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/Raytracing/VolumetricCloudScapes/VolumetricCloudScapesShader.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class IndicesSystem;

	/// [EN] Mirrors Raytracing/VolumetricCloudScapes/VolumetricCloudScapes.hlsli's
	///      VolumetricCloudScapesRayConstantBuffer — read by both
	///      VolumetricCloudScapesRT.hlsl and DeferredLightingPS.hlsl via
	///      structured_indices.cloud_ray_constant_index_. Must stay
	///      byte-for-byte in sync with the HLSL side.
	/// [JP] Raytracing/VolumetricCloudScapes/VolumetricCloudScapes.hlsli の
	///      VolumetricCloudScapesRayConstantBuffer と対応。
	///      VolumetricCloudScapesRT.hlsl と DeferredLightingPS.hlsl の両方が
	///      structured_indices.cloud_ray_constant_index_ 経由で読む。HLSL 側と
	///      バイト単位で一致させること。以下は 4 スカラー = cbuffer 1 行
	///      (16 バイト)単位で並べてある — フィールドを足すときもこの区切りを
	///      崩さないこと。
	struct VolumetricCloudScapesRayConstantBuffer
	{
		/// [EN] Cloud layer altitudes, measured above the CURVED ground
		///      (see CloudAltitudeAt) rather than as an infinite flat slab.
		/// [JP] 雲層の高度。無限平板のワールドYではなく、曲率つき地面からの
		///      高度(CloudAltitudeAt 参照)。
		Float cloudBottom_ = 500.0f;
		Float cloudTop_ = 1500.0f;

		/// [EN] Cloud amount 0..1.
		/// [JP] 雲量 0..1。
		Float coverage_ = 0.5f;

		/// [EN] Extinction scale of the density field.
		/// [JP] 密度場の消散スケール。
		Float densityScale_ = 0.02f;

		Float cloudAlbedo_[3] = { 1.0f, 1.0f, 1.0f };

		/// [EN] Henyey-Greenstein anisotropy of the FORWARD lobe (0 = isotropic,
		///      ~0.6 = forward). The backward lobe is phaseBackward_/phaseBlend_.
		/// [JP] Henyey-Greenstein の異方性(前方ローブ、0=等方、~0.6=前方散乱)。
		///      後方ローブは phaseBackward_/phaseBlend_ 側。
		Float scatteringG_ = 0.6f;

		/// [EN] View sample budget (spent adaptively by the marcher) and sun
		///      lightmarch steps.
		///
		///      stepCount_ sets the fine step to layerThickness/stepCount_, so 64
		///      over the default 1000-unit layer is a 15.6-unit step against a
		///      finest shape-noise feature of ~39 units - still comfortably above
		///      Nyquist, and 33% cheaper than 96.
		///
		///      lightStepCount_ is the expensive one: each view sample inside
		///      cloud spends this many extra density taps, so it dominates the
		///      pass. The steps grow geometrically, so 6 still spans the layer.
		/// [JP] 視線側のサンプル予算(マーチャーが適応的に使う)と
		///      太陽ライトマーチのステップ数。
		///
		///      stepCount_ は細ステップ長 = 層の厚み/stepCount_ を決める。既定の
		///      1000 単位の層に対して 64 なら 1 ステップ 15.6 単位で、形状ノイズの
		///      最小特徴 約39 単位に対して十分細かい(かつ 96 より 33% 安い)。
		///
		///      lightStepCount_ が高コスト側。雲の中の視線サンプル1つごとに
		///      この回数の密度タップを追加で消費するので、このパスの支配項。
		///      ステップは指数的に伸びるので 6 でも層を張れる。
		Uint32 stepCount_ = 64;
		Uint32 lightStepCount_ = 6;

		/// [EN] World position -> shape noise domain scale.
		/// [JP] ワールド座標→形状ノイズ域のスケール(小さいほど雲の塊が大きい)。
		Float noiseScale_ = 0.0008f;

		/// [EN] Wind scroll speed (noise domain units per second).
		/// [JP] 風のスクロール速度(ノイズ域単位/秒)。
		Float windSpeed_ = 0.01f;

		/// [EN] Incremented once per frame by VolumetricCloudScapesRenderer
		///      (not the UI). Only rotates the raymarch start dither, and only
		///      while temporalJitter_ is non-zero.
		/// [JP] VolumetricCloudScapesRenderer が毎フレーム1つずつ加算する
		///      (UI からは触らない)。レイマーチ開始ディザの回転にのみ使い、
		///      しかも temporalJitter_ が 0 でない間だけ効く。
		Uint32 frameIndex_ = 0;

		/// [EN] 0 = stratus (flat, low band), 1 = cumulus (tall, billowing).
		/// [JP] 0=層雲(低くて平らな帯)、1=積雲(縦に発達したもこもこ)。
		Float cloudType_ = 1.0f;

		/// [EN] 0 = fair weather, 1 = rain clouds (dark, dense, high coverage).
		/// [JP] 0=晴天、1=雨雲(暗い・濃い・雲量増)。
		Float rain_ = 0.0f;

		/// [EN] Detail noise domain scale relative to the shape scale.
		/// [JP] 形状ノイズに対するディテールノイズ域のスケール倍率。
		///      ディテールボリュームの最上位オクターブを 32→16 セルに落として
		///      エイリアスを取った分、既定値を倍にしてワールド空間での
		///      ディテール周波数を揃えている。
		Float detailScale_ = 8.0f;

		/// [EN] Procedural sky (used only when no skymap is bound).
		/// [JP] プロシージャル空(スカイマップ未バインド時のみ使用)。
		///      天頂色=「空の青さ」、地平線色、太陽の視半径、全体の明るさ、
		///      地平線より下の地面色。
		Float skyZenithColor_[3] = { 0.15f, 0.35f, 0.75f };
		Float sunSize_ = 0.03f;

		Float skyHorizonColor_[3] = { 0.65f, 0.75f, 0.9f };
		Float skyBrightness_ = 1.0f;

		Float groundColor_[3] = { 0.25f, 0.22f, 0.2f };

		/// [EN] 1 while the cloud/sky feature is enabled. Set by
		///      VolumetricCloudScapesRenderer (not the UI) — the composite
		///      shader reads it to decide whether skymap-less backgrounds get
		///      the procedural sky.
		/// [JP] 機能が有効な間 1。VolumetricCloudScapesRenderer が設定する
		///      (UI からは触らない) — コンポジットがスカイマップ無しの背景に
		///      プロシージャル空を描くかの判定に読む。
		Uint32 proceduralSkyEnabled_ = 0;

		/// [EN] Planet radius in world units. The cloud layer is a shell around
		///      a sphere of this radius, so it bends down and converges at the
		///      horizon instead of running to infinity as a flat slab. Larger =
		///      flatter.
		/// [JP] 惑星半径(ワールド単位)。雲層をこの半径の球殻として扱うため、
		///      遠方で層が下がって地平線に収束する(無限平板だと地平線に硬い
		///      境目が出る)。大きいほど平ら。
		Float planetRadius_ = 6360000.0f;

		/// [EN] Far limit of the cloud raymarch. Alpha is faded out toward it so
		///      the cut is invisible.
		/// [JP] 雲レイマーチの遠方上限。上限へ向けて alpha をフェードするので
		///      切れ目は見えない。
		Float maxMarchDistance_ = 120000.0f;

		/// [EN] Distance at which step length starts growing and detail noise
		///      starts fading (stands in for a mip chain the volumes lack).
		/// [JP] ステップ長を伸ばし始め、ディテールノイズをフェードし始める距離
		///      (ノイズボリュームに mip が無いのでその代替)。
		Float lodDistance_ = 25000.0f;

		/// [EN] Aerial perspective: 1/e distance for washing distant clouds
		///      toward the horizon sky color.
		/// [JP] 空気遠近。遠景の雲を地平線色へ寄せる減衰の 1/e 距離。
		Float aerialDensity_ = 0.000012f;

		/// [EN] Weather field frequency RELATIVE to noiseScale_. Smaller = larger
		///      cloud masses.
		/// [JP] weather フィールドの周波数(noiseScale_ に対する相対値)。
		///      小さいほど雲の塊が大きくなる。
		Float weatherScale_ = 0.06f;

		/// [EN] How hard the weather field pushes coverage around, 0..1.
		///      0 restores the old globally-uniform coverage.
		/// [JP] weather が雲量をどれだけ動かすか 0..1。0 で従来の
		///      「空全体で一定の雲量」に戻る。
		Float weatherAmount_ = 0.6f;

		/// [EN] Detail erosion amount.
		/// [JP] ディテールノイズが雲の輪郭を削る量。
		Float detailStrength_ = 0.35f;

		/// [EN] Detail scroll speed relative to windSpeed_. The detail volume
		///      needs its OWN offset — folding the shape's wind offset through
		///      detailScale_ drags detail across the cloud body at detailScale_
		///      times the wind speed, which reads as the surface boiling.
		/// [JP] ディテールのスクロール速度(windSpeed_ に対する相対値)。
		///      ディテールは独自オフセットで動かす必要がある — 形状側の風
		///      オフセットを detailScale_ 倍してしまうと、ディテールが雲の
		///      本体に対して detailScale_ 倍の速度で滑り、表面が沸騰している
		///      ように見える。
		Float detailWindSpeed_ = 0.3f;

		/// [EN] Backward lobe anisotropy (positive, negated in the shader) and
		///      the blend weight between forward and backward lobes. A single
		///      lobe can give either the silver lining or the bright down-sun
		///      face, never both.
		/// [JP] 後方ローブの異方性(正値、シェーダ側で符号反転)と、前方/後方
		///      ローブのブレンド率。単一ローブでは逆光のシルバーライニングと
		///      順光の明るい面のどちらか一方しか出せない。
		Float phaseBackward_ = 0.35f;
		Float phaseBlend_ = 0.4f;

		/// [EN] Beer-powder strength 0..1: darkens the sunlit surface of a
		///      cloud, which is what makes its shape readable instead of a flat
		///      saturated blob.
		/// [JP] Beer-powder の強さ 0..1。日向側の表面を暗く落とし、白飛びした
		///      塊ではなく形が読める見た目にする。
		Float powderStrength_ = 1.0f;

		/// [EN] Sky-gradient ambient multiplier (keeps undersides off black).
		/// [JP] 空グラデーション環境光の倍率(雲底が真っ黒に潰れないように)。
		Float ambientStrength_ = 0.4f;

		/// [EN] Multiple-scattering octaves, and the per-octave scales for
		///      extinction / contribution / phase eccentricity. This is what
		///      fills in cloud interiors — single scattering physically cannot,
		///      no matter how the density is tuned.
		///
		///      These values are an ENERGY budget, not just a look knob. A thick
		///      cloud is not a thin scattering medium: a photon scatters dozens
		///      of times with almost no absorption before it leaves, so the
		///      cloud behaves as a near-Lambertian reflector of albedo ~0.9,
		///      radiating about 0.9*E/pi = 0.286*E. A single backscatter lobe is
		///      only ~0.11*E, and the octave sum has to close that gap.
		///
		///      contribution 0.75 over 5 octaves sums to 3.05, which lands a
		///      sunlit cloud face at roughly 0.33*E — right on the 0.318*E a
		///      sunlit white Lambertian surface renders at in this engine
		///      (BrdfLambertian carries the 1/pi). The old 0.5 over 3 octaves
		///      summed to 1.75 and left clouds visibly darker than lit ground.
		///      Costs arithmetic only: the optical depth is marched once.
		/// [JP] 多重散乱のオクターブ数と、消散/寄与/位相偏心のオクターブごとの
		///      減衰率。雲の内部を埋めるのはこの項で、単一散乱では密度を
		///      どういじっても再現できない。
		///
		///      これは見た目のツマミではなく【エネルギー配分】。厚い雲は薄い
		///      散乱媒質ではなく、ほぼ吸収なしで数十回散乱してから出ていくので、
		///      アルベド 0.9 程度のほぼランバート反射体として振る舞い、
		///      0.9*E/pi = 0.286*E ほどの輝度を持つ。単一の後方散乱ローブは
		///      0.11*E 程度しかなく、その差をオクターブ列の総和で埋める。
		///
		///      寄与 0.75 × 5 オクターブ = 総和 3.05。日向の雲面が約 0.33*E に
		///      なり、このエンジンで日向の白いランバート面が出る 0.318*E
		///      (BrdfLambertian が 1/pi を含む)とほぼ一致する。旧設定の
		///      0.5 × 3 オクターブは総和 1.75 で、日向の地面より明らかに暗かった。
		///      追加コストは演算のみ(光学的深さのマーチは1回で使い回す)。
		Uint32 multiScatterOctaves_ = 5;
		Float multiScatterAttenuation_ = 0.5f;
		Float multiScatterContribution_ = 0.75f;
		Float multiScatterEccentricity_ = 0.5f;

		/// [EN] 0 = frame-independent dither (stable, no crawling grain).
		///      1 = full per-frame rotation, for use behind TAA/DLSS.
		/// [JP] 0=フレーム非依存ディザ(安定、grain が這わない)。
		///      1=毎フレーム変動、TAA/DLSS で解決させる前提。
		Float temporalJitter_ = 0.0f;

		Float cloudPadding0_ = 0.0f;
		Float cloudPadding1_ = 0.0f;
		Float cloudPadding2_ = 0.0f;

		template<class Archive>
		void serialize(Archive& archive)
		{
			TryLoadField(archive, "cloudBottom", cloudBottom_);
			TryLoadField(archive, "cloudTop", cloudTop_);
			TryLoadField(archive, "coverage", coverage_);
			TryLoadField(archive, "densityScale", densityScale_);
			TryLoadField(archive, "cloudAlbedo", cloudAlbedo_);
			TryLoadField(archive, "scatteringG", scatteringG_);
			TryLoadField(archive, "stepCount", stepCount_);
			TryLoadField(archive, "lightStepCount", lightStepCount_);
			TryLoadField(archive, "noiseScale", noiseScale_);
			TryLoadField(archive, "windSpeed", windSpeed_);
			TryLoadField(archive, "cloudType", cloudType_);
			TryLoadField(archive, "rain", rain_);
			TryLoadField(archive, "detailScale", detailScale_);
			TryLoadField(archive, "skyZenithColor", skyZenithColor_);
			TryLoadField(archive, "sunSize", sunSize_);
			TryLoadField(archive, "skyHorizonColor", skyHorizonColor_);
			TryLoadField(archive, "skyBrightness", skyBrightness_);
			TryLoadField(archive, "groundColor", groundColor_);
			TryLoadField(archive, "planetRadius", planetRadius_);
			TryLoadField(archive, "maxMarchDistance", maxMarchDistance_);
			TryLoadField(archive, "lodDistance", lodDistance_);
			TryLoadField(archive, "aerialDensity", aerialDensity_);
			TryLoadField(archive, "weatherScale", weatherScale_);
			TryLoadField(archive, "weatherAmount", weatherAmount_);
			TryLoadField(archive, "detailStrength", detailStrength_);
			TryLoadField(archive, "detailWindSpeed", detailWindSpeed_);
			TryLoadField(archive, "phaseBackward", phaseBackward_);
			TryLoadField(archive, "phaseBlend", phaseBlend_);
			TryLoadField(archive, "powderStrength", powderStrength_);
			TryLoadField(archive, "ambientStrength", ambientStrength_);
			TryLoadField(archive, "multiScatterOctaves", multiScatterOctaves_);
			TryLoadField(archive, "multiScatterAttenuation", multiScatterAttenuation_);
			TryLoadField(archive, "multiScatterContribution", multiScatterContribution_);
			TryLoadField(archive, "multiScatterEccentricity", multiScatterEccentricity_);
			TryLoadField(archive, "temporalJitter", temporalJitter_);
		}
	};

	/// [EN] The HLSL mirror is 12 cbuffer rows of 4 scalars. Nothing catches a
	///      layout drift at runtime — every shader would just read garbage from
	///      the field after the one that moved — so assert the size here.
	/// [JP] HLSL 側は 4 スカラー × 12 行。レイアウトがずれても実行時には
	///      検出できず、ずれた位置以降のフィールドを全シェーダが化けた値として
	///      読むだけなので、サイズをここで静的検証する。
	static_assert(sizeof(VolumetricCloudScapesRayConstantBuffer) == 12 * 4 * sizeof(Float), "VolumetricCloudScapesRayConstantBuffer が VolumetricCloudScapes.hlsli とバイト単位で一致していません");

	/**
	* [EN]
	* Dispatches the volumetric cloud scapes compute pass
	* (VolumetricCloudScapesRT.hlsl — screen-space raymarch over a procedural
	* density field, sky pixels only) into an RGBA16F texture (rgb =
	* in-scattered radiance, a = coverage) and leaves it in
	* PIXEL_SHADER_RESOURCE state for DeferredLightingPS.hlsl to composite
	* over the skybox. Needs no TLAS (pure raymarch); like
	* SubsurfaceScatteringRenderer there is NO denoiser and NO per-view chain
	* — one texture is written and consumed within each view's flush.
	* When disabled the texture is cleared to 0 (no clouds).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボリューメトリック・クラウドスケープのコンピュートパス
	* (VolumetricCloudScapesRT.hlsl — プロシージャル密度場のスクリーン空間
	* レイマーチ、空ピクセル限定)を RGBA16F テクスチャ(rgb=内散乱放射輝度、
	* a=カバレッジ)へディスパッチし、DeferredLightingPS.hlsl がスカイボックス
	* の上に合成できるよう PIXEL_SHADER_RESOURCE 状態にしておく。TLAS 不要
	* (純レイマーチ)。SubsurfaceScatteringRenderer と同じくデノイザも
	* ビュー別チェーンも無い — 各ビューの Flush 内で書いて読む1枚。
	* 無効時はテクスチャを 0(雲なし)でクリアする。
	*/
	class VolumetricCloudScapesRenderer
	{
	public:
		VolumetricCloudScapesRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~VolumetricCloudScapesRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		/// [EN] Updates the tuning constant buffer (stamping enabled into
		///      proceduralSkyEnabled_) and registers its bindless indices into
		///      IndicesSystem. Must run before IndicesSystem::
		///      UploadEditor/UploadGame bakes this frame's indices. No GPU work.
		/// [JP] チューニング用定数バッファを更新し(enabled を
		///      proceduralSkyEnabled_ に焼き込む)、bindless インデックスを
		///      IndicesSystem へ登録する。IndicesSystem::UploadEditor/UploadGame
		///      が今フレームのインデックスを確定する前に呼ぶこと。GPU 処理は無い。
		void PrepareFrame(const VolumetricCloudScapesRayConstantBuffer& settings, Bool enabled);

		/// [EN] The actual GPU work: dispatches the raymarch (or clears the
		///      texture to 0 if enabled is false or the PSO is missing),
		///      leaving it in PIXEL_SHADER_RESOURCE state. Requires the
		///      G-Buffer depth to already be written (sky-pixel test).
		/// [JP] 実際の GPU 処理: レイマーチをディスパッチする(enabled が false
		///      か PSO が無ければ 0 でクリア)。テクスチャは
		///      PIXEL_SHADER_RESOURCE 状態で終える。G-Buffer の深度が書き込み
		///      済みであることが前提(空ピクセル判定)。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool enabled);

	private:
		/// [JP] ノイズ Texture3D を1枚作る(UAV+SRV、ベイク用)。
		void CreateNoiseTexture(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 size, Microsoft::WRL::ComPtr<ID3D12Resource>& outResource, Uint32& outUnorderedAccessViewIndex, Uint32& outShaderResourceViewIndex);

		/// [EN] The cloud texture is rendered at 1/N of the screen in each axis
		///      and bilinearly upsampled by DeferredLightingPS.hlsl. Clouds are
		///      a low-frequency signal, so this is nearly free visually and cuts
		///      the pass by N^2 — measured at 26ms full-res, which is the whole
		///      frame budget. Unreal does the same (it goes to 1/4 plus temporal
		///      reprojection). Raise to 4 for another 4x if 2 is not enough; the
		///      only cost is softer cloud edges.
		/// [JP] 雲テクスチャは画面の 1/N 解像度で描き、DeferredLightingPS.hlsl が
		///      バイリニアで拡大する。雲は低周波な信号なので見た目の代償はごく
		///      小さく、コストは N^2 分の1になる — フル解像度で実測 26ms、
		///      フレーム予算そのものだった。Unreal も同じ手法(あちらは 1/4 +
		///      テンポラル再投影)。2 で足りなければ 4 にすればさらに 4 倍速く
		///      なる。代償は雲の輪郭が甘くなることだけ。
		static constexpr Uint32 resolutionDivisor_ = 1;

		static constexpr Uint32 shapeNoiseSize = 128;
		static constexpr Uint32 detailNoiseSize = 64;

		VolumetricCloudScapesShader cloudShader_;

		ResourcePtr<ConstantBuffer<VolumetricCloudScapesRayConstantBuffer>> tuningBuffer_;

		Microsoft::WRL::ComPtr<ID3D12Resource> cloudResource_;
		D3D12_RESOURCE_STATES cloudState_ = D3D12_RESOURCE_STATE_COMMON;
		Uint32 cloudUnorderedAccessViewIndex_ = 0;
		Uint32 cloudShaderResourceViewIndex_ = 0;

		/// [EN] Pre-baked tileable noise volumes (Perlin-Worley shape 128^3,
		///      Worley detail 64^3, R16_FLOAT). Baked once on the first
		///      Dispatch() (needs a command list), then wrap-sampled by the
		///      raymarch. R16_FLOAT rather than R8_UNORM because CloudDensity's
		///      coverage remap divides by coverage_, which amplifies the
		///      quantisation step by 1/coverage_ and terraces soft cloud edges.
		/// [JP] ベイク済みタイル可能ノイズボリューム(形状 Perlin-Worley 128^3、
		///      ディテール Worley 64^3、R16_FLOAT)。コマンドリストが要るため
		///      最初の Dispatch() で一度だけ焼き、以後レイマーチが wrap
		///      サンプルする。R8_UNORM ではなく R16_FLOAT にしているのは、
		///      CloudDensity の雲量リマップが coverage_ で除算するため量子化幅が
		///      1/coverage_ 倍に増幅され、柔らかい雲縁が階段状になるから。
		Microsoft::WRL::ComPtr<ID3D12Resource> shapeNoiseResource_;
		Uint32 shapeNoiseUnorderedAccessViewIndex_ = 0;
		Uint32 shapeNoiseShaderResourceViewIndex_ = 0;

		Microsoft::WRL::ComPtr<ID3D12Resource> detailNoiseResource_;
		Uint32 detailNoiseUnorderedAccessViewIndex_ = 0;
		Uint32 detailNoiseShaderResourceViewIndex_ = 0;

		Bool noiseBaked_ = false;

		/// [EN] Non-shader-visible UAV descriptor required by
		///      ClearUnorderedAccessViewFloat alongside the shader-visible one.
		/// [JP] ClearUnorderedAccessViewFloat がシェーダ可視の UAV と併せて
		///      要求する、非シェーダ可視の UAV ディスクリプタ。
		DescriptorHeap clearHeap_;
		Uint32 clearIndex_ = 0;

		BindlessHeap* bindlessHeap_ = nullptr;
		IndicesSystem* indicesSystem_ = nullptr;

		/// [EN] Cloud texture resolution — the SCREEN size divided by
		///      resolutionDivisor_, not the screen size itself.
		/// [JP] 雲テクスチャの解像度。画面サイズそのものではなく、
		///      resolutionDivisor_ で割った後のサイズ。
		Uint32 width_ = 0;
		Uint32 height_ = 0;

		Uint32 frameIndex_ = 0;

		/// [EN] Logs the PSO-creation-failed warning once instead of every frame.
		/// [JP] PSO 作成失敗の警告を毎フレームでなく 1 度だけログ出力する。
		Bool pipelineStateMissingLogged_ = false;
	};
}

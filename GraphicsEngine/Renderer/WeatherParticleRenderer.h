#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/SerializeFallback.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/Environment/WeatherParticleShader.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class IndicesSystem;
	class FrameBuffer;
	class GeometryBuffer;

	/// [EN] Tuning for the rain particle system, editable from グラフィックス→
	///      環境→雨. density_ scales how much of the fixed particle pool is
	///      active, multiplied every frame by the current weather rain_
	///      amount - so it fades out naturally as rain stops instead of
	///      needing a separate enable toggle for "is it raining".
	/// [JP] 雨パーティクル系の調整値、グラフィックス→環境→雨から編集する。
	///      density_ は固定パーティクルプールのうちどれだけを有効にするかの
	///      係数で、毎フレーム現在の天候の rain_ 量と掛け合わされる - 「今
	///      降っているか」用の別トグルを持たなくても、雨が止めば自然に
	///      フェードアウトする。
	struct RainSettings
	{
		Float density_ = 1.0f;
		Float fallSpeed_ = 9.0f;
		Float size_ = 0.035f;
		Float streakLength_ = 0.035f;
		Float brightness_ = 0.8f;
		Float volumeRadius_ = 14.0f;
		Float volumeHeight_ = 10.0f;
		Float color_[3] = { 0.75f, 0.8f, 0.9f };

		template<class Archive>
		void serialize(Archive& archive)
		{
			TryLoadField(archive, "density", density_);
			TryLoadField(archive, "fallSpeed", fallSpeed_);
			TryLoadField(archive, "size", size_);
			TryLoadField(archive, "streakLength", streakLength_);
			TryLoadField(archive, "brightness", brightness_);
			TryLoadField(archive, "volumeRadius", volumeRadius_);
			TryLoadField(archive, "volumeHeight", volumeHeight_);
			TryLoadField(archive, "color", color_);
		}
	};

	/// [EN] Same shape as RainSettings, for snow (see WeatherSystem's
	///      snowIntensity_ - the fast "is it snowing now" signal this scales
	///      against, distinct from snowCoverage_'s slow ground accumulation).
	/// [JP] RainSettings と同じ形、雪用(掛け合わせる相手は WeatherSystem の
	///      snowIntensity_ - 「今降っているか」の速い信号。地面の積雪
	///      snowCoverage_ とは別)。
	struct SnowSettings
	{
		Float density_ = 1.0f;
		Float fallSpeed_ = 1.3f;
		Float size_ = 0.05f;
		Float swayAmount_ = 0.4f;
		Float brightness_ = 0.9f;
		Float volumeRadius_ = 12.0f;
		Float volumeHeight_ = 9.0f;
		Float color_[3] = { 1.0f, 1.0f, 1.0f };

		template<class Archive>
		void serialize(Archive& archive)
		{
			TryLoadField(archive, "density", density_);
			TryLoadField(archive, "fallSpeed", fallSpeed_);
			TryLoadField(archive, "size", size_);
			TryLoadField(archive, "swayAmount", swayAmount_);
			TryLoadField(archive, "brightness", brightness_);
			TryLoadField(archive, "volumeRadius", volumeRadius_);
			TryLoadField(archive, "volumeHeight", volumeHeight_);
			TryLoadField(archive, "color", color_);
		}
	};

	/// [EN] Mirrors Raytracing.../Environment/WeatherParticle.hlsli's
	///      WeatherParticle byte-for-byte.
	/// [JP] Environment/WeatherParticle.hlsli の WeatherParticle とバイト単位で一致。
	struct WeatherParticle
	{
		Vector3 position_ = { 0.0f, 0.0f, 0.0f };
		Float life_ = 0.0f;
		Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
		Float seed_ = 0.0f;
	};

	/// [EN] Mirrors Environment/WeatherParticle.hlsli's
	///      WeatherParticleConstantBuffer byte-for-byte - one combined buffer
	///      driving both the rain and snow dispatches/draws (see that file for
	///      why they share one buffer instead of two).
	/// [JP] Environment/WeatherParticle.hlsli の WeatherParticleConstantBuffer
	///      とバイト単位で一致 - 雨/雪のディスパッチ・描画を1つの
	///      バッファでまとめて駆動する(理由は同ファイル参照)。
	struct WeatherParticleConstantBuffer
	{
		Vector3 cameraPosition_ = { 0.0f, 0.0f, 0.0f };
		Float deltaTime_ = 0.0f;

		Vector3 wind_ = { 0.0f, 0.0f, 0.0f };
		Float totalTime_ = 0.0f;

		Uint32 forceRespawn_ = 0;
		Uint32 rainCapacity_ = 0;
		Uint32 rainActiveCount_ = 0;
		Float rainFallSpeed_ = 0.0f;

		Float rainSize_ = 0.0f;
		Float rainStreakLength_ = 0.0f;
		Float rainBrightness_ = 0.0f;
		Float rainVolumeRadius_ = 0.0f;

		Float rainVolumeHeight_ = 0.0f;
		Vector3 rainColor_ = { 0.0f, 0.0f, 0.0f };

		Uint32 snowCapacity_ = 0;
		Uint32 snowActiveCount_ = 0;
		Float snowFallSpeed_ = 0.0f;
		Float snowSwayAmount_ = 0.0f;

		Float snowSize_ = 0.0f;
		Float snowBrightness_ = 0.0f;
		Float snowVolumeRadius_ = 0.0f;
		Float snowVolumeHeight_ = 0.0f;

		Vector3 snowColor_ = { 0.0f, 0.0f, 0.0f };
		Float weatherParticlePadding0_ = 0.0f;
	};
	static_assert(sizeof(WeatherParticleConstantBuffer) == 8 * 4 * sizeof(Float), "WeatherParticleConstantBuffer が WeatherParticle.hlsli とバイト単位で一致していません");

	/**
	* [EN]
	* Owns the GPU-simulated rain/snow particle systems: two fixed-capacity
	* StructuredBuffers (DEFAULT heap, UAV+SRV - CPU never touches them after
	* creation), advanced every frame by a compute pass
	* (WeatherParticleSimulateCS.hlsl - gravity/wind + camera-relative
	* recycling, see that file), then drawn via an AS+MS+PS mesh-shader
	* pipeline (WeatherParticleAS/MS/PS.hlsl) with depth TEST but no write, so
	* the existing G-buffer depth occludes particles per-pixel - that hardware
	* depth test is the "collision with models". No CPU-side particle list:
	* everything after the initial spawn lives and dies on the GPU.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* GPUでシミュレーションする雨/雪パーティクル系を保持する: 固定容量の
	* StructuredBuffer 2本(DEFAULTヒープ、UAV+SRV - 生成後はCPUが触らない)を、
	* コンピュートパス(WeatherParticleSimulateCS.hlsl - 重力/風 +
	* カメラ相対の再スポーン、同ファイル参照)で毎フレーム進め、AS+MS+PSの
	* メッシュシェーダパイプライン(WeatherParticleAS/MS/PS.hlsl)で描画する
	* (深度テストのみ・書込み無しなので、既存のG-Buffer深度が画素単位で
	* パーティクルを遮蔽する - このハードウェア深度テストが「モデルとの
	* 衝突」)。CPU側のパーティクルリストは持たない: 初期スポーン以降は
	* 全てGPU上で生き死にする。
	*/
	class WeatherParticleRenderer
	{
	public:
		WeatherParticleRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~WeatherParticleRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem);

		void Destroy(BindlessHeap* bindlessHeap);

		/// [EN] Updates the tuning constant buffer, registers bindless indices
		///      into IndicesSystem. Must run before IndicesSystem::
		///      UploadEditor/UploadGame bakes this frame's indices. No GPU work.
		/// [JP] チューニング用定数バッファを更新し、bindless インデックスを
		///      IndicesSystem へ登録する。IndicesSystem::UploadEditor/
		///      UploadGame が今フレームのインデックスを確定する前に呼ぶこと。
		///      GPU 処理は無い。
		void PrepareFrame(const Vector3& cameraPosition, Float deltaTime, Float totalTime, const Vector3& wind, Bool rainEnabled, const RainSettings& rainSettings, Float rainAmount, Bool snowEnabled, const SnowSettings& snowSettings, Float snowAmount);

		/// [EN] The compute simulate pass. Needs no G-Buffer, so it can run any
		///      time before Draw().
		/// [JP] コンピュートのシミュレートパス。G-Buffer 不要なので Draw() より
		///      前ならいつ実行してもよい。
		void Simulate(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		/// [EN] The mesh-shader draw pass: binds frameBuffer's color RTV +
		///      geometryBuffer's depth DSV directly (mirrors ModelRenderer::
		///      DrawWireframe's binding), same as any other forward-style pass
		///      drawn after the opaque composite. Caller must already be inside
		///      a geometryBuffer->BeginDepth()/EndDepth() scope (read-only
		///      depth state) - this method does not manage that transition
		///      itself, matching DrawWireframe/DrawMeshlet's convention.
		/// [JP] メッシュシェーダの描画パス: frameBuffer の色RTV +
		///      geometryBuffer の深度DSVを直接バインドする(ModelRenderer::
		///      DrawWireframe と同じバインド方式)、不透明合成後に描かれる他の
		///      フォワード系パスと同様。呼び出し側は既に
		///      geometryBuffer->BeginDepth()/EndDepth() スコープ内である
		///      こと(深度読み取り専用状態) - この関数自体はその遷移を管理
		///      しない(DrawWireframe/DrawMeshlet と同じ規約)。
		void Draw(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, GeometryBuffer* geometryBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

	private:
		void CreateParticleBuffer(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 capacity, Microsoft::WRL::ComPtr<ID3D12Resource>& outResource, Uint32& outUnorderedAccessViewIndex, Uint32& outShaderResourceViewIndex);

	private:
		static constexpr Uint32 rainCapacity_ = 10000;
		static constexpr Uint32 snowCapacity_ = 6000;

		WeatherParticleShader particleShader_;

		ResourcePtr<ConstantBuffer<WeatherParticleConstantBuffer>> tuningBuffer_;

		Microsoft::WRL::ComPtr<ID3D12Resource> rainParticleResource_;
		D3D12_RESOURCE_STATES rainParticleState_ = D3D12_RESOURCE_STATE_COMMON;
		Uint32 rainParticleUnorderedAccessViewIndex_ = 0;
		Uint32 rainParticleShaderResourceViewIndex_ = 0;

		Microsoft::WRL::ComPtr<ID3D12Resource> snowParticleResource_;
		D3D12_RESOURCE_STATES snowParticleState_ = D3D12_RESOURCE_STATE_COMMON;
		Uint32 snowParticleUnorderedAccessViewIndex_ = 0;
		Uint32 snowParticleShaderResourceViewIndex_ = 0;

		Bool initialized_ = false;
		Uint32 activeTotal_ = 0;

		BindlessHeap* bindlessHeap_ = nullptr;
		IndicesSystem* indicesSystem_ = nullptr;

		Bool pipelineStateMissingLogged_ = false;
	};
}

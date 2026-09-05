#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>

namespace SeedCore
{
	class BindlessHeap;
	class World;
	class ShaderCache;
	class RootSignature;
	class PipelineStateObject;
	class ComputeShader;
	class D3D12CommandList;
	struct LoaderSystem;
	class ModelResource;
	struct CelestialResult;
	struct WeatherGpuState;

	struct LightConstantBuffer
	{
		Vector3 directionalDirection_ = { 0.0f, -1.0f, 0.0f };
		Float directionalIntensity_ = 0.0f;
		Color directionalColor_ = { 0,0,0,0 };

		/// [EN] Moon light, populated only when CelestialSystem drives this frame
		///      (see LightSystem::Gather's celestial parameter). Zero otherwise.
		/// [JP] 月ライト。CelestialSystem がこのフレームを駆動する時のみ書かれる
		///      (LightSystem::Gather の celestial 引数参照)。それ以外は0。
		Vector3 moonDirection_ = { 0.0f, 1.0f, 0.0f };
		Float moonIntensity_ = 0.0f;
		Color moonColor_ = { 0,0,0,0 };

		/// [EN] 0 = full day, 1 = full night. Drives VolumetricStar visibility.
		/// [JP] 0=完全な昼、1=完全な夜。VolumetricStar の見え方を駆動する。
		Float moonPhase_ = 0.0f;
		Float nightFactor_ = 0.0f;
		Float moonAngularRadius_ = 0.02f;
		Float lightConstantBufferPadding1_ = 0.0f;

		Uint pointLightCount_ = 0;
		Uint spotLightCount_ = 0;
		Uint rectLightCount_ = 0;
		Uint pointLightShaderResourceViewIndex_ = 0;
		Uint spotLightShaderResourceViewIndex_ = 0;
		Uint rectLightShaderResourceViewIndex_ = 0;
		Uint clusterDataShaderResourceViewIndex_ = 0;
		Uint clusterLightListShaderResourceViewIndex_ = 0;
		Uint clusterCountX_ = 0;
		Uint clusterCountY_ = 0;
		Vector2 lightConstantBufferPadding0_;

		/// [EN] Weather state (see WeatherSystem::ReadGpuState). Zero when the
		///      scene has no Weather component.
		/// [JP] 天候の状態(WeatherSystem::ReadGpuState 参照)。シーンに Weather
		///      コンポーネントが無ければ0。
		Float wetness_ = 0.0f;
		Float snowCoverage_ = 0.0f;
		Float thunderFlash_ = 0.0f;
		Float lightConstantBufferPadding2_ = 0.0f;

		/// [EN] snowIntensity_ drives the falling-snow screen overlay ("is it
		///      snowing right now", fast ramp - unlike snowCoverage_'s slow
		///      ground accumulation). thunderSeed_ randomizes the lightning
		///      bolt shape, re-rolled each strike.
		/// [JP] snowIntensity_ は降雪の画面オーバーレイを駆動する
		///      (「今降っているか」、素早く増減 - snowCoverage_ の遅い積雪とは別)。
		///      thunderSeed_ は稲妻の形を発生ごとに変える乱数シード。
		Float snowIntensity_ = 0.0f;
		Float thunderSeed_ = 0.0f;
		Vector2 lightConstantBufferPadding3_;
	};

	struct ClusterAssignConstantBuffer
	{
		Uint clusterDataUnorderedAccessViewIndex_ = 0;
		Uint clusterLightListUnorderedAccessViewIndex_ = 0;
		Uint pointLightShaderResourceViewIndex_ = 0;
		Uint spotLightShaderResourceViewIndex_ = 0;
		Uint rectLightShaderResourceViewIndex_ = 0;
		Uint pointLightCount_ = 0;
		Uint spotLightCount_ = 0;
		Uint rectLightCount_ = 0;
		Uint totalClusters_ = 0;
		Uint clusterCountX_ = 0;
		Uint clusterCountY_ = 0;
		Float clusterAssignConstantBufferPadding0_;
		Float nearPlane_ = 0.1f;
		Float farPlane_ = 1000.0f;
		Vector2 clusterAssignConstantBufferPadding1_;
	};

	struct PointLightData
	{
		Vector3 position_;
		Float range_;
		Color color_;
		Float intensity_;
		Vector3 pointLightDataPadding0_;
	};

	struct SpotLightData
	{
		Vector3 position_;
		Float range_;
		Vector3 direction_;
		Float cosHalfAngle_;
		Color color_;
		Float intensity_;
		Float softness_;
		Vector2 spotLightDataPadding0_;
	};

	struct RectLightData
	{
		Vector3 position_;
		Float intensity_;
		Vector3 right_;
		Float halfWidth_;
		Vector3 up_;
		Float halfHeight_;
		Vector3 normal_;
		Float range_;
		Color color_;
	};

	struct ClusterData
	{
		Uint pointCount_ = 0;
		Uint spotCount_ = 0;
		Uint rectCount_ = 0;
		Float clusterDataPadding0_ = 0.0f;
	};

	class LightSystem
	{
	public:
		LightSystem(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, RootSignature& rootSignature, PipelineStateObject& pipelineStateObject, Uint32 width, Uint32 height);
		~LightSystem() = default;

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		/// [EN] celestial, when non-null, overrides the sun (skips the scene's
		///      DirectionalLight query) and writes the moon fields; when null the
		///      scene's DirectionalLight drives the sun as before and the moon
		///      fields stay zero.
		/// [JP] celestial が非nullの場合、太陽をそれで上書きし(シーンの
		///      DirectionalLight クエリをスキップ)、月フィールドも書く。null なら
		///      従来通りシーンの DirectionalLight が太陽を駆動し、月フィールドは
		///      0のまま。
		/// [EN] weather, when non-null, writes wetness_/snowCoverage_/thunderFlash_;
		///      when null they stay zero (no Weather component in the scene).
		/// [JP] weather が非nullの場合、wetness_/snowCoverage_/thunderFlash_ を
		///      書く。null ならそのまま0(シーンに Weather コンポーネント無し)。
		void Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, World& world, const CelestialResult* celestial = nullptr, const WeatherGpuState* weather = nullptr);

		void Upload();

		void DispatchCluster(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredAddress);

		[[nodiscard]] Uint GetIndex()const;

		[[nodiscard]] Uint GetClusterConstantIndex()const;

		[[nodiscard]] Float GetDirectionalIntensity()const;

	private:
		void CreateClusterResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

	private:
		static constexpr Uint maxPointLights_ = 65536;
		static constexpr Uint maxSpotLights_ = 65536;
		static constexpr Uint maxRectLights_ = 4096;
		static constexpr Uint clusterTileSize_ = 64;
		static constexpr Uint clusterDepthSlices_ = 16;
		static constexpr Uint clusterMaxPointLights_ = 64;
		static constexpr Uint clusterMaxSpotLights_ = 64;
		static constexpr Uint clusterMaxRectLights_ = 64;
		static constexpr Uint clusterStride_ = clusterMaxPointLights_ + clusterMaxSpotLights_ + clusterMaxRectLights_;

		LightConstantBuffer lightConstantData_;
		ClusterAssignConstantBuffer clusterAssignConstantData_;

		DynamicArray<PointLightData> pointLights_;
		DynamicArray<SpotLightData> spotLights_;
		DynamicArray<RectLightData> rectLights_;

		ResourcePtr<ConstantBuffer<LightConstantBuffer>> lightConstantBuffer_;
		ResourcePtr<ConstantBuffer<ClusterAssignConstantBuffer>> clusterAssignConstantBuffer_;

		ResourcePtr<ReadOnlyStructuredBuffer<PointLightData>> pointLightBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<SpotLightData>> spotLightBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<RectLightData>> rectLightBuffer_;

		Microsoft::WRL::ComPtr<ID3D12Resource> clusterDataResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> clusterLightListResource_;

		DescriptorHeap clearHeap_;


		Uint clusterDataUnorderedAccessViewIndex_ = 0;
		Uint clusterDataShaderResourceViewIndex_ = 0;
		Uint clusterDataClearIndex_ = 0;
		Uint clusterDataClearUnorderedAccessViewIndex_ = 0;
		Uint clusterLightListUnorderedAccessViewIndex_ = 0;
		Uint clusterLightListShaderResourceViewIndex_ = 0;
		Uint clusterLightListClearIndex_ = 0;

		Uint totalClusters_ = 0;
		Uint clusterCountX_ = 0;
		Uint clusterCountY_ = 0;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> clusterAssignPipelineStateObject_;
		RootSignature* clusterRootSignature_;

		BindlessHeap* bindlessHeap_ = nullptr;

		Handle<ComputeShader> clusterAssignShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> clusterAssignPipelineStateObjectHandle_;
		Handle<RootSignature> rootSignatureHandle_;
	};
}

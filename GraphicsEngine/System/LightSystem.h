#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Log/Assert.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
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

	struct DirectionalLightConstantBuffer
	{
		Vector3 direction_;
		Float directionalLightPadding0_ = 0.0f;

		Float sunIntensity_ = 0.0f;
		Float sunAngularRadius_ = 0.02f;
		Uint directionalLightPadding1_[2] = { 0, 0 };
		Color sunColor_ = { 0, 0, 0, 0 };

		Float moonIntensity_ = 0.0f;
		Float moonAngularRadius_ = 0.02f;
		Uint directionalLightPadding2_[2] = { 0, 0 };
		Color moonColor_ = { 0, 0, 0, 0 };

		Float moonPhase_ = 0.0f;
		Uint directionalLightPadding3_[3] = { 0, 0, 0 };
	};
	SC_STATIC_ASSERT_SIZE(DirectionalLightConstantBuffer, 96, "Light/Light.hlsli");

	struct PointLightStructuredBuffer
	{
		Vector3 position_;
		Float range_;
		Color color_;
		Float intensity_;
		Vector3 pointLightStructuredBufferPadding0_;
	};

	struct SpotLightStructuredBuffer
	{
		Vector3 position_;
		Float range_;
		Vector3 direction_;
		Float cosHalfAngle_;
		Color color_;
		Float intensity_;
		Float softness_;
		Vector2 spotLightStructuredBufferPadding0_;
	};

	struct RectLightStructuredBuffer
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

	struct LightConstantBuffer
	{
		Float nightFactor_ = 0.0f;
		Uint pointLightCount_ = 0;
		Uint spotLightCount_ = 0;
		Uint rectLightCount_ = 0;

		Uint clusterCountX_ = 0;
		Uint clusterCountY_ = 0;
		Uint lightConstantPadding0_[2] = { 0, 0 };
	};
	SC_STATIC_ASSERT_SIZE(LightConstantBuffer, 32, "Light/Light.hlsli");

	struct LightShaderResourceIndices
	{
		Uint pointLightIndex_ = 0;
		Uint spotLightIndex_ = 0;
		Uint rectLightIndex_ = 0;
		Uint clusterDataIndex_ = 0;

		Uint clusterLightListIndex_ = 0;
		Uint lightShaderResourcePadding0_[3] = { 0, 0, 0 };
	};
	SC_STATIC_ASSERT_SIZE(LightShaderResourceIndices, 32, "Light/Cluster.hlsli");

	struct ClusterAssignConstantBuffer
	{
		Uint pointLightCount_ = 0;
		Uint spotLightCount_ = 0;
		Uint rectLightCount_ = 0;
		Uint totalClusters_ = 0;

		Uint clusterCountX_ = 0;
		Uint clusterCountY_ = 0;
		Float nearPlane_ = 0.1f;
		Float farPlane_ = 1000.0f;
	};
	SC_STATIC_ASSERT_SIZE(ClusterAssignConstantBuffer, 32, "Light/Cluster.hlsli");

	struct ClusterAssignShaderResourceIndices
	{
		Uint pointLightIndex_ = 0;
		Uint spotLightIndex_ = 0;
		Uint rectLightIndex_ = 0;
		Uint clusterAssignShaderResourcePadding0_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(ClusterAssignShaderResourceIndices, 16, "Light/Cluster.hlsli");

	struct ClusterAssignUnorderedAccessIndices
	{
		Uint clusterDataIndex_ = 0;
		Uint clusterLightListIndex_ = 0;
		Uint clusterAssignUnorderedAccessPadding0_[2] = { 0, 0 };
	};
	SC_STATIC_ASSERT_SIZE(ClusterAssignUnorderedAccessIndices, 16, "Light/Cluster.hlsli");

	struct ClusterInstance
	{
		Uint pointCount_ = 0;
		Uint spotCount_ = 0;
		Uint rectCount_ = 0;
		Float clusterInstancePadding0_ = 0.0f;
	};

	class LightSystem
	{
	public:
		LightSystem(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, RootSignature& rootSignature, PipelineStateObject& pipelineStateObject, Uint32 width, Uint32 height);
		~LightSystem() = default;

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		void Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, World& world, const CelestialResult* celestial = nullptr);

		void Upload();

		void DispatchCluster(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		[[nodiscard]] Uint GetIndex()const;

		[[nodiscard]] Uint GetDirectionalLightIndex()const;

		[[nodiscard]] Uint GetClusterAssignIndex()const;

		[[nodiscard]] LightShaderResourceIndices GetLightShaderResourceIndices()const;

		[[nodiscard]] ClusterAssignShaderResourceIndices GetClusterAssignShaderResourceIndices()const;

		[[nodiscard]] ClusterAssignUnorderedAccessIndices GetClusterAssignUnorderedAccessIndices()const;

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
		DirectionalLightConstantBuffer directionalLightConstantData_;
		ClusterAssignConstantBuffer clusterAssignConstantData_;

		DynamicArray<PointLightStructuredBuffer> pointLights_;
		DynamicArray<SpotLightStructuredBuffer> spotLights_;
		DynamicArray<RectLightStructuredBuffer> rectLights_;

		ResourcePtr<ConstantBuffer<LightConstantBuffer>> lightConstantBuffer_;
		ResourcePtr<ConstantBuffer<DirectionalLightConstantBuffer>> directionalLightConstantBuffer_;
		ResourcePtr<ConstantBuffer<ClusterAssignConstantBuffer>> clusterAssignConstantBuffer_;

		ResourcePtr<ReadOnlyStructuredBuffer<PointLightStructuredBuffer>> pointLightBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<SpotLightStructuredBuffer>> spotLightBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<RectLightStructuredBuffer>> rectLightBuffer_;

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

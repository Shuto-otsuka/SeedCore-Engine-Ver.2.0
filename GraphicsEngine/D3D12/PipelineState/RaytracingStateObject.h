#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
#pragma pack(push, 1)
	struct RaytracingStateKey
	{
		ID3D12RootSignature* globalRootSignature_;
		ID3D12RootSignature* localRootSignature_;

		D3D12_SHADER_BYTECODE libraryShader_;

		String rayGenExportName_;
		String missExportName_;
		String closestHitExportName_;
		String hitGroupName_;

		Uint32 maxTraceRecursionDepth_ = 1;
		Uint32 maxAttributeSizeInBytes_ = 8;
		Uint32 maxPayloadSizeInBytes_ = 16;
	};
#pragma pack(pop)

	class SEEDCORE_API RaytracingStateObject
	{
	private:
		struct RaytracingStateKeyHash
		{
			Size operator()(const RaytracingStateKey& key)const noexcept
			{
				return std::hash<std::string_view>{}(std::string_view(reinterpret_cast<const Char*>(&key), sizeof(key)));
			}
		};

		struct RaytracingStateKeyEqual
		{
			Bool operator()(const RaytracingStateKey& lhs, const RaytracingStateKey& rhs)const noexcept
			{
				return std::memcmp(&lhs, &rhs, sizeof(RaytracingStateKey)) == 0;
			}
		};

	public:
		RaytracingStateObject() = default;
		~RaytracingStateObject();

		Handle<Microsoft::WRL::ComPtr<ID3D12StateObject>> GetOrCreate(ID3D12Device5* device, const RaytracingStateKey& key);

		[[nodiscard]] ID3D12StateObject* Get(const Handle<Microsoft::WRL::ComPtr<ID3D12StateObject>>& handle);

	private:
		FlatMap<RaytracingStateKey, Handle<Microsoft::WRL::ComPtr<ID3D12StateObject>>, RaytracingStateKeyHash, RaytracingStateKeyEqual> raytracingState_;

		StablePool<Microsoft::WRL::ComPtr<ID3D12StateObject>> pool_;
	};
}

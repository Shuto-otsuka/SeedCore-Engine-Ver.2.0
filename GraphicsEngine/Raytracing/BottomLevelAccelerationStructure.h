#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/// [EN] Description of a single triangle geometry fed into a BLAS build.
	///      Addresses point at the raw vertex/index buffers (the same data the
	///      rasterizer uses); by default (ModelLoader::ConvertAxisConvention /
	///      Crister::ApplyAxisConversion with WindingOverride::Auto) index
	///      order is never touched, matching the implicit winding flip caused
	///      by vertex mirroring alone - only an explicit WindingOverride::Flip
	///      swaps index order, so this invariant only breaks if that override
	///      is misused for a given asset's actual axis convention.
	/// [JP] BLAS 構築へ渡す単一の三角形ジオメトリ記述。アドレスはラスタと同じ
	///      生の頂点/インデックスバッファを指す。既定では
	///      （ModelLoader::ConvertAxisConvention / Crister::ApplyAxisConversion
	///      の WindingOverride::Auto）インデックス順は一切触らない。これは
	///      頂点ミラーだけで暗黙的に起きる巻き順反転と一致する — 明示的に
	///      WindingOverride::Flip を選んだ時だけインデックス順を入れ替えるため、
	///      その選択がアセットの実際の軸コンベンションと矛盾しない限り、
	///      この不変条件は崩れない。
	struct BottomLevelGeometryDesc
	{
		D3D12_GPU_VIRTUAL_ADDRESS vertexBuffer_ = 0;
		Uint32 vertexCount_ = 0;
		Uint32 vertexStride_ = 0;
		DXGI_FORMAT vertexFormat_ = DXGI_FORMAT_R32G32B32_FLOAT;

		D3D12_GPU_VIRTUAL_ADDRESS indexBuffer_ = 0;
		Uint32 indexCount_ = 0;
		DXGI_FORMAT indexFormat_ = DXGI_FORMAT_R32_UINT;

		Bool opaque_ = true;
	};

	/// [EN] Bottom-Level Acceleration Structure: the BVH over one mesh's triangles,
	///      built once in model-local space and referenced by TLAS instances.
	/// [JP] Bottom-Level Acceleration Structure: 1 メッシュの三角形群に対する BVH。
	///      モデルローカル空間で 1 度構築し、TLAS のインスタンスから参照する。
	class BottomLevelAccelerationStructure :public NonCopyable
	{
	public:
		BottomLevelAccelerationStructure() = default;
		~BottomLevelAccelerationStructure() = default;

		Bool Build(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, const BottomLevelGeometryDesc* geometries, Uint32 geometryCount);

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS Address()const noexcept;

		[[nodiscard]] ID3D12Resource* Resource()const noexcept;

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> result_;
		Microsoft::WRL::ComPtr<ID3D12Resource> scratch_;

		Uint64 resultCapacity_ = 0;
		Uint64 scratchCapacity_ = 0;

		Bool invalidGeometryLogged_ = false;
	};
}

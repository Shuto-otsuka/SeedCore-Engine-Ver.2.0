#include <GraphicsEngine/Raytracing/BottomLevelAccelerationStructure.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	namespace
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> CreateAccelerationStructureBuffer(ID3D12Device5* device, Uint64 size, D3D12_RESOURCE_STATES state)
		{
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProperties.CreationNodeMask = 1;
			heapProperties.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = size;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			/// [EN] Soft failure: a null return makes Build() bail out, the caller
			///      drops this mesh from the TLAS instance list, and rendering
			///      continues without it. Hard-failing here (modal box +
			///      __debugbreak) would fire once per allocation while a removed
			///      device fails every call in a row.
			/// [JP] ソフトフェイル: null を返すと Build() が中断し、呼び出し側が
			///      このメッシュを TLAS インスタンス一覧から外して描画を継続する。
			///      ここでハードフェイル(モーダル + __debugbreak)すると、デバイス削除で
			///      全呼び出しが連続して失敗する状況では確保のたびに発火してしまう。
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, state, nullptr, IID_PPV_ARGS(&resource));
			if (FAILED(hr))
			{
				return nullptr;
			}
			return resource;
		}
	}

	Bool BottomLevelAccelerationStructure::Build(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, const BottomLevelGeometryDesc* geometries, Uint32 geometryCount)
	{
		if (geometries == nullptr || geometryCount == 0)
		{
			return false;
		}

		DynamicArray<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
		geometryDescs.reserve(geometryCount);
		for (Uint32 geometryIndex = 0; geometryIndex < geometryCount; geometryIndex++)
		{
			const BottomLevelGeometryDesc& source = geometries[geometryIndex];

			/// [EN] Reject geometry the build cannot make sense of before handing
			///      it to DXR. A null vertex address (a Crister whose GPU buffers
			///      are not uploaded yet, or whose upload failed) makes the build
			///      read from GPU VA 0, which yields a BVH of degenerate triangles
			///      rather than a page fault - and traversal over that never
			///      terminates, hanging the GPU inside a later DispatchRays with
			///      nothing to point at. A triangle list also needs its vertex or
			///      index count to be a whole number of triangles.
			/// [JP] DXR へ渡す前に、構築が意味を成さないジオメトリを弾く。頂点
			///      アドレスが null(GPU バッファ未アップロード、あるいは
			///      アップロード失敗の Crister)だと、構築は GPU 仮想アドレス 0 から
			///      読み、ページフォルトではなく退化三角形だらけの BVH ができる -
			///      その走査は終了せず、後段の DispatchRays の中で手がかりも無いまま
			///      GPU がハングする。三角形リストは頂点数/インデックス数が
			///      三角形単位で割り切れる必要もある。
			Bool indexed = source.indexBuffer_ != 0 && source.indexCount_ > 0;
			Bool valid = source.vertexBuffer_ != 0 && source.vertexCount_ > 0 && source.vertexStride_ > 0;
			if (valid)
			{
				valid = indexed ? (source.indexCount_ % 3 == 0) : (source.vertexCount_ % 3 == 0);
			}

			if (!valid)
			{
				if (!invalidGeometryLogged_)
				{
					SC_LOG_WARNING("BLAS のジオメトリが不正なため構築を中止しました。(vertexBuffer={:#x} vertexCount={} vertexStride={} indexBuffer={:#x} indexCount={})", source.vertexBuffer_, source.vertexCount_, source.vertexStride_, source.indexBuffer_, source.indexCount_);
					invalidGeometryLogged_ = true;
				}
				return false;
			}

			D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc{};
			geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			geometryDesc.Flags = source.opaque_ ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
			geometryDesc.Triangles.VertexBuffer.StartAddress = source.vertexBuffer_;
			geometryDesc.Triangles.VertexBuffer.StrideInBytes = source.vertexStride_;
			geometryDesc.Triangles.VertexCount = source.vertexCount_;
			geometryDesc.Triangles.VertexFormat = source.vertexFormat_;
			geometryDesc.Triangles.IndexBuffer = source.indexBuffer_;
			geometryDesc.Triangles.IndexCount = source.indexBuffer_ ? source.indexCount_ : 0;
			geometryDesc.Triangles.IndexFormat = source.indexBuffer_ ? source.indexFormat_ : DXGI_FORMAT_UNKNOWN;
			geometryDesc.Triangles.Transform3x4 = 0;
			geometryDescs.push_back(geometryDesc);
		}

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		inputs.NumDescs = geometryCount;
		inputs.pGeometryDescs = geometryDescs.data();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
		device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
		if (prebuildInfo.ResultDataMaxSizeInBytes == 0)
		{
			return false;
		}

		/// [EN] Reuse the existing buffers whenever they are still large enough:
		///      a skinned/morphed BLAS rebuilds every frame, so allocating fresh
		///      committed resources here would mean two CreateCommittedResource
		///      calls per animated instance per frame - the same churn the
		///      skinned/morphed position buffers already avoid with a capacity
		///      check (see RaytracingRenderer::Build).
		/// [JP] 既存バッファがまだ十分な大きさなら再利用する: スキン/モーフ付きの
		///      BLAS は毎フレーム再構築されるため、ここで毎回コミット済み
		///      リソースを確保すると、アニメーションするインスタンス1体につき
		///      毎フレーム CreateCommittedResource 2回になってしまう -
		///      スキン/モーフ位置バッファが容量チェックで既に避けているのと
		///      同じ churn(RaytracingRenderer::Build 参照)。
		if (scratchCapacity_ < prebuildInfo.ScratchDataSizeInBytes)
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> scratch = CreateAccelerationStructureBuffer(device, prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_COMMON);
			if (!scratch)
			{
				/// [EN] Leave scratch_/result_ untouched on failure: a BLAS that built
				///      successfully on an earlier frame stays valid and keeps rendering
				///      instead of being dropped from the TLAS.
				/// [JP] 失敗時は scratch_/result_ に触れない: 過去フレームで構築に成功した
				///      BLAS はそのまま有効に保ち、TLAS から外さず描画を継続する。
				return false;
			}

			scratch_ = scratch;
			scratchCapacity_ = prebuildInfo.ScratchDataSizeInBytes;
#ifdef _DEBUG
			scratch_->SetName(L"BottomLevelAccelerationStructure_Scratch");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(scratch_.Get());
#endif
		}

		if (resultCapacity_ < prebuildInfo.ResultDataMaxSizeInBytes)
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> result = CreateAccelerationStructureBuffer(device, prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
			if (!result)
			{
				return false;
			}

			result_ = result;
			resultCapacity_ = prebuildInfo.ResultDataMaxSizeInBytes;
#ifdef _DEBUG
			result_->SetName(L"BottomLevelAccelerationStructure_Result");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(result_.Get());
#endif
		}

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
		buildDesc.Inputs = inputs;
		buildDesc.ScratchAccelerationStructureData = scratch_->GetGPUVirtualAddress();
		buildDesc.DestAccelerationStructureData = result_->GetGPUVirtualAddress();

		commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

		/// [EN] The TLAS build and any ray query reading this BLAS must wait for the
		///      build writes to complete; a UAV barrier on the result enforces that.
		/// [JP] この BLAS を読む TLAS 構築やレイクエリは、構築書き込みの完了を待つ
		///      必要がある。result への UAV バリアでそれを保証する。
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = result_.Get();
		commandList->ResourceBarrier(1, &barrier);

		return true;
	}

	D3D12_GPU_VIRTUAL_ADDRESS BottomLevelAccelerationStructure::Address()const noexcept
	{
		return result_ ? result_->GetGPUVirtualAddress() : 0;
	}

	ID3D12Resource* BottomLevelAccelerationStructure::Resource()const noexcept
	{
		return result_.Get();
	}
}

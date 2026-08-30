#include <GraphicsEngine/D3D12/PipelineState/RaytracingStateObject.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	RaytracingStateObject::~RaytracingStateObject()
	{
		raytracingState_.clear();
	}

	Handle<Microsoft::WRL::ComPtr<ID3D12StateObject>> RaytracingStateObject::GetOrCreate(ID3D12Device5* device, const RaytracingStateKey& key)
	{
		if (auto found = raytracingState_.find(key))
		{
			return found->second;
		}

		HRESULT hr{ S_OK };

		DynamicArray<D3D12_STATE_SUBOBJECT> subobjects;
		subobjects.reserve(8);

		std::wstring rayGenName = key.rayGenExportName_.w_str();
		std::wstring missName = key.missExportName_.w_str();
		std::wstring closesHitName = key.closestHitExportName_.w_str();
		std::wstring anyHitName = key.anyHitExportName_.w_str();
		std::wstring hitGroupName = key.hitGroupName_.w_str();

		D3D12_DXIL_LIBRARY_DESC dxilDesc{};
		dxilDesc.DXILLibrary = key.libraryShader_;

		DynamicArray<D3D12_EXPORT_DESC> exports;
		if (!rayGenName.empty())
		{
			exports.push_back({ rayGenName.c_str(), nullptr, D3D12_EXPORT_FLAG_NONE });
		}
		if (!missName.empty())
		{
			exports.push_back({ missName.c_str(), nullptr, D3D12_EXPORT_FLAG_NONE });
		}
		if (!closesHitName.empty())
		{
			exports.push_back({ closesHitName.c_str(), nullptr, D3D12_EXPORT_FLAG_NONE });
		}
		if (!anyHitName.empty())
		{
			exports.push_back({ anyHitName.c_str(), nullptr, D3D12_EXPORT_FLAG_NONE });
		}

		dxilDesc.NumExports = static_cast<Uint>(exports.size());
		dxilDesc.pExports = exports.data();

		D3D12_STATE_SUBOBJECT dxilSubobject{};
		dxilSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		dxilSubobject.pDesc = &dxilDesc;
		subobjects.push_back(dxilSubobject);

		D3D12_HIT_GROUP_DESC hitGroupDesc{};
		if (!hitGroupName.empty())
		{
			hitGroupDesc.HitGroupExport = hitGroupName.c_str();
			hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			if (!closesHitName.empty())
			{
				hitGroupDesc.ClosestHitShaderImport = closesHitName.c_str();
			}
			if (!anyHitName.empty())
			{
				hitGroupDesc.AnyHitShaderImport = anyHitName.c_str();
			}

			D3D12_STATE_SUBOBJECT hitGroupSub{};
			hitGroupSub.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
			hitGroupSub.pDesc = &hitGroupDesc;
			subobjects.push_back(hitGroupSub);
		}

		D3D12_RAYTRACING_SHADER_CONFIG shaderConfig{};
		shaderConfig.MaxPayloadSizeInBytes = key.maxPayloadSizeInBytes_;
		shaderConfig.MaxAttributeSizeInBytes = key.maxAttributeSizeInBytes_;

		D3D12_STATE_SUBOBJECT configSub{};
		configSub.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
		configSub.pDesc = &shaderConfig;
		subobjects.push_back(configSub);

		D3D12_LOCAL_ROOT_SIGNATURE localRootSig{};
		D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION association{};
		const Wchar* exportList[] = { hitGroupName.c_str() };

		if (key.localRootSignature_)
		{
			localRootSig.pLocalRootSignature = key.localRootSignature_;

			D3D12_STATE_SUBOBJECT localSigSub{};
			localSigSub.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
			localSigSub.pDesc = &localRootSig;
			subobjects.push_back(localSigSub);

			association.pSubobjectToAssociate = &subobjects.back();
			association.NumExports = 1;
			association.pExports = exportList;

			D3D12_STATE_SUBOBJECT associationSub{};
			associationSub.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
			associationSub.pDesc = &association;
			subobjects.push_back(associationSub);
		}

		D3D12_GLOBAL_ROOT_SIGNATURE globalRootSignature{};
		globalRootSignature.pGlobalRootSignature = key.globalRootSignature_;

		D3D12_STATE_SUBOBJECT globalSignatureSub{};
		globalSignatureSub.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
		globalSignatureSub.pDesc = &globalRootSignature;
		subobjects.push_back(globalSignatureSub);

		D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig{};
		pipelineConfig.MaxTraceRecursionDepth = key.maxTraceRecursionDepth_;

		D3D12_STATE_SUBOBJECT pipelineSub{};
		pipelineSub.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
		pipelineSub.pDesc = &pipelineConfig;
		subobjects.push_back(pipelineSub);

		D3D12_STATE_OBJECT_DESC stateObjectDesc{};
		stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
		stateObjectDesc.NumSubobjects = static_cast<UINT>(subobjects.size());
		stateObjectDesc.pSubobjects = subobjects.data();

		Microsoft::WRL::ComPtr<ID3D12StateObject> raytracingStateObject;
		hr = device->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(&raytracingStateObject));
		SC_HR_CHECK(hr, "RTPSO(CreateStateObject)の生成に失敗しました");

		if (FAILED(hr) || !raytracingStateObject)
		{
			return Handle<Microsoft::WRL::ComPtr<ID3D12StateObject>>::null();
		}

		auto handle = pool_.Create();
		if (auto psoPtr = pool_.Get(handle))
		{
			*psoPtr = std::move(raytracingStateObject);
		}

		raytracingState_.insert({ key, handle });

		return handle;
	}

	ID3D12StateObject* RaytracingStateObject::Get(const Handle<Microsoft::WRL::ComPtr<ID3D12StateObject>>& handle)
	{
		if (auto psoPtr = pool_.Get(handle))
		{
			return psoPtr->Get();
		}
		return nullptr;
	}
}
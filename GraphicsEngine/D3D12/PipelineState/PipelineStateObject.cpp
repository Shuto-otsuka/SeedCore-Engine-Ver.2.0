#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>

namespace SeedCore
{
	namespace
	{
		template<typename T, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE SubType>
		struct alignas(void*) PipelineStateStreamSubobject
		{
			D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_ = SubType;
			T value_{};
		};

		struct MeshShaderPipelineStateStream
		{
			PipelineStateStreamSubobject<ID3D12RootSignature*,    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE>     rootSignature;
			PipelineStateStreamSubobject<D3D12_SHADER_BYTECODE,   D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS>                 as;
			PipelineStateStreamSubobject<D3D12_SHADER_BYTECODE,   D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS>                 ms;
			PipelineStateStreamSubobject<D3D12_SHADER_BYTECODE,   D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS>                 ps;
			PipelineStateStreamSubobject<D3D12_RASTERIZER_DESC,   D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER>         rasterizer;
			PipelineStateStreamSubobject<D3D12_BLEND_DESC,        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND>              blend;
			PipelineStateStreamSubobject<D3D12_DEPTH_STENCIL_DESC,D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL>      depthStencil;
			PipelineStateStreamSubobject<D3D12_RT_FORMAT_ARRAY,   D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS> rtvFormats;
			PipelineStateStreamSubobject<DXGI_FORMAT,             D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT> dsvFormat;
			PipelineStateStreamSubobject<DXGI_SAMPLE_DESC,        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC>        sampleDesc;
			PipelineStateStreamSubobject<UINT,                    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK>        sampleMask;
		};
	}

	PipelineStateObject::~PipelineStateObject()
	{
		pipelineState_.clear();
	}

	Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> PipelineStateObject::GetOrCreate(ID3D12Device* device, const PipelineStateKey& key)
	{
		if (auto found = pipelineState_.find(key))
		{
			return found->second;
		}

		HRESULT hr{ S_OK };
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateObject;

		if (key.computeShader_.pShaderBytecode != nullptr)
		{
			D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateObjectDesc{};
			pipelineStateObjectDesc.pRootSignature = key.rootSignature_;
			pipelineStateObjectDesc.CS = key.computeShader_;
			pipelineStateObjectDesc.NodeMask = 0;

			hr = device->CreateComputePipelineState(&pipelineStateObjectDesc, IID_PPV_ARGS(&pipelineStateObject));
		}
		else if (key.meshShader_.pShaderBytecode != nullptr)
		{
			MeshShaderPipelineStateStream stream{};
			stream.rootSignature.value_ = key.rootSignature_;
			stream.as.value_ = key.amplificationShader_;
			stream.ms.value_ = key.meshShader_;
			stream.ps.value_ = key.pixelShader_;
			stream.rasterizer.value_ = key.rasterizerDesc_;
			stream.blend.value_ = key.blendDesc_;
			stream.depthStencil.value_ = key.depthStencilDesc_;
			stream.rtvFormats.value_.NumRenderTargets = key.renderTargetViewCount_;
			std::memcpy(stream.rtvFormats.value_.RTFormats, key.renderTargetViewFormat_, sizeof(key.renderTargetViewFormat_));
			stream.dsvFormat.value_ = key.depthStencilViewFormat_;
			stream.sampleDesc.value_ = { 1, 0 };
			stream.sampleMask.value_ = UINT_MAX;

			D3D12_PIPELINE_STATE_STREAM_DESC streamDesc{};
			streamDesc.SizeInBytes = sizeof(stream);
			streamDesc.pPipelineStateSubobjectStream = &stream;

			Microsoft::WRL::ComPtr<ID3D12Device2> device2;
			hr = device->QueryInterface(IID_PPV_ARGS(&device2));
			if (SUCCEEDED(hr))
			{
				hr = device2->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&pipelineStateObject));
			}
		}
		else
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateObjectDesc{};
			pipelineStateObjectDesc.pRootSignature = key.rootSignature_;
			pipelineStateObjectDesc.VS = key.vertexShader_;
			pipelineStateObjectDesc.PS = key.pixelShader_;
			pipelineStateObjectDesc.DS = key.domainShader_;
			pipelineStateObjectDesc.HS = key.hullShader_;
			pipelineStateObjectDesc.GS = key.geometryShader_;

			pipelineStateObjectDesc.RasterizerState = key.rasterizerDesc_;
			pipelineStateObjectDesc.BlendState = key.blendDesc_;
			pipelineStateObjectDesc.DepthStencilState = key.depthStencilDesc_;
			pipelineStateObjectDesc.NumRenderTargets = key.renderTargetViewCount_;
			pipelineStateObjectDesc.DSVFormat = key.depthStencilViewFormat_;
			pipelineStateObjectDesc.PrimitiveTopologyType = key.primitiveTopologyType_;

			pipelineStateObjectDesc.SampleMask = UINT_MAX;
			pipelineStateObjectDesc.SampleDesc.Count = 1;
			pipelineStateObjectDesc.SampleDesc.Quality = 0;

			std::memcpy(pipelineStateObjectDesc.RTVFormats, key.renderTargetViewFormat_, sizeof(pipelineStateObjectDesc.RTVFormats));

			hr = device->CreateGraphicsPipelineState(&pipelineStateObjectDesc, IID_PPV_ARGS(&pipelineStateObject));
		}

		if (FAILED(hr) || !pipelineStateObject)
		{
			return Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>>::null();
		}

		auto handle = pool_.Create();
		if (auto psoPtr = pool_.Get(handle))
		{
			*psoPtr = std::move(pipelineStateObject);
		}

		pipelineState_.insert({ key, handle });

		return handle;
	}

	ID3D12PipelineState* PipelineStateObject::Get(const Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>>& handle)
	{
		if (auto psoPtr = pool_.Get(handle))
		{
			return psoPtr->Get();
		}
		return nullptr;
	}
}

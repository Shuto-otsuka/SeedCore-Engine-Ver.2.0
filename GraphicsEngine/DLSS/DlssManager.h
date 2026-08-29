#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
#include <GraphicsEngine/DLSS/DlssMode.h>
#include <GraphicsEngine/System/SceneSystem.h>

namespace SeedCore
{
	struct DlssLoadResult
	{
		Bool isDlss_ = false;
		Bool isReflex_ = false;
		Bool isDeepDVC_ = false;
		Bool isDlssFG_ = false;
		Bool isDlssRR_ = false;
	};

	struct DlssBufferTag
	{
		ID3D12Resource* colorBuffer_ = nullptr;
		ID3D12Resource* depthBuffer_ = nullptr;
		ID3D12Resource* albedoBuffer_ = nullptr;
		ID3D12Resource* velocityBuffer_ = nullptr;
		ID3D12Resource* normalRoughnessBuffer_ = nullptr;
		ID3D12Resource* specularAlbedoBuffer_ = nullptr;

		D3D12_RESOURCE_STATES colorBufferState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		D3D12_RESOURCE_STATES depthBufferState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		D3D12_RESOURCE_STATES albedoBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		D3D12_RESOURCE_STATES velocityBufferState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		D3D12_RESOURCE_STATES normalRoughnessBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		D3D12_RESOURCE_STATES specularAlbedoBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		Float width_ = ScResolution::SC_HD.Width;
		Float height_ = ScResolution::SC_HD.Height;
	};

	class SEEDCORE_API DlssManager
	{
	public:
		DlssManager() = default;
		~DlssManager() = default;

		Bool Initialize();

		void Finalize();

	public:
		Bool Prepare(ID3D12Device* device);

		void BeginFrame();

		void RayReconstructionTag(DlssBufferTag tags, ID3D12CommandList* cmdList, ID3D12Resource* outPutBuffer, Uint32 viewportIndex, Uint32 outputWidth, Uint32 outputHeight);

		void FrameGenerationTag(DlssBufferTag tags, ID3D12CommandList* cmdList, ID3D12Resource* hudlessResource, ID3D12Resource* uiColorAlphaResource, Uint32 viewportIndex, Uint32 outputWidth, Uint32 outputHeight);

		void Constants(const SceneConstantBuffer& scene, Uint32 viewportIndex, Bool reset);

	public:
		void ReflexEnable(Bool enable);

		Bool ReflexEnable()const;

		void DeepDVCEnable(Bool enable);

		Bool DeepDVCEnable()const;

		void FrameGenerationEnable(Bool enable);

		Bool FrameGenerationEnable()const;

		void FrameGenerationSuppress(Bool suppress);

		Bool FrameGenerationVSyncSupported()const;

		void RayReconstructionEnable(Bool enable);

		Bool RayReconstructionEnable()const;

	public:
		void Reflex(Bool useBoost);

		void DeepDVC(Float intensity, Float saturationBoost);

	public:
		void EvaluateReflex();

		void EvaluateDeepDVC(ID3D12CommandList* cmdList, ID3D12Resource* colorBuffer, Uint32 viewportIndex, Uint32 width, Uint32 height);

		void EvaluateFrameGeneration(Uint32 viewportIndex);

		void EvaluateRayReconstruction(ID3D12CommandList* cmdList, const SceneConstantBuffer& scene, Uint32 viewportIndex, Uint32 outputWidth, Uint32 outputHeight, UpscaleMode mode);

		void EvaluateDlss(ID3D12CommandList* cmdList, Uint32 viewportIndex, Uint32 outputWidth, Uint32 outputHeight, UpscaleMode mode);

	private:
		DlssLoadResult loadResult_{};

		Bool reflexEnabled_ = false;
		Bool reflexUseBoost_ = false;

		Bool deepDVCEnabled_ = false;
		Float deepDVCIntensity_ = 0.5f;
		Float deepDVCSaturationBoost_ = 0.25f;

		Bool frameGenerationEnabled_ = false;
		Bool frameGenerationSuppressed_ = false;
		Bool frameGenerationVSyncSupported_ = false;

		Bool rayReconstructionEnabled_ = false;

#if !SC_RENDER_DOC_USAGE
		sl::ResourceTag tags_[7]{};
		sl::Constants constants_{};
		sl::FrameToken* currentToken_{ nullptr };
#endif
	};

#if !SC_RENDER_DOC_USAGE
	inline void ScDlssErrorCallback(sl::LogType type, const Char* message)
	{
		if (type != sl::LogType::eError)
		{
			return;
		}

		Char buffer[512];
		wsprintfA(buffer, "[DLSS Error]: %s\n", message);
		OutputDebugStringA(buffer);
	}
#endif
}
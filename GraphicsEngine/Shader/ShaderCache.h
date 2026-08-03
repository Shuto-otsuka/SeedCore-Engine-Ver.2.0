#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/ArtMap.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/NonTransferable.h>

namespace SeedCore
{
	class VertexShader;
	class HullShader;
	class DomainShader;
	class GeometryShader;
	class PixelShader;
	class AmplificationShader;
	class MeshShader;
	class RaytracingShader;
	class ComputeShader;

	class ShaderCache :public NonTransferable
	{
	public:
#pragma pack(push, 1)
		struct Key
		{
			String filePath_;
			String entryPoint_;
		};
#pragma pack(pop)

	public:
		ShaderCache() = default;
		~ShaderCache() = default;

		[[nodiscard]] Handle<VertexShader> GetOrCreateVertexShader(String filePath, String entryPoint = String("main"));

		[[nodiscard]] VertexShader* GetVertexShader(const Handle<VertexShader>& handle)noexcept;

		[[nodiscard]] Handle<HullShader> GetOrCreateHullShader(String filePath, String entryPoint = String("main"));

		[[nodiscard]] HullShader* GetHullShader(const Handle<HullShader>& handle)noexcept;

		[[nodiscard]] Handle<DomainShader> GetOrCreateDomainShader(String filePath, String entryPoint = String("main"));

		[[nodiscard]] DomainShader* GetDomainShader(const Handle<DomainShader>& handle)noexcept;

		[[nodiscard]] Handle<GeometryShader> GetOrCreateGeometryShader(String filePath, String entryPoint = String("main"));

		[[nodiscard]] GeometryShader* GetGeometryShader(const Handle<GeometryShader>& handle)noexcept;

		[[nodiscard]] Handle<PixelShader> GetOrCreatePixelShader(String filePath, String entryPoint = String("main"));

		[[nodiscard]] PixelShader* GetPixelShader(const Handle<PixelShader>& handle)noexcept;

		[[nodiscard]] Handle<AmplificationShader> GetOrCreateAmplificationShader(String filePath, String entryPoint = String("main"));

		[[nodiscard]] AmplificationShader* GetAmplificationShader(const Handle<AmplificationShader>& handle)noexcept;

		[[nodiscard]] Handle<MeshShader> GetOrCreateMeshShader(String filePath, String entryPoint = String("main"));

		[[nodiscard]] MeshShader* GetMeshShader(const Handle<MeshShader>& handle)noexcept;

		[[nodiscard]] Handle<RaytracingShader> GetOrCreateRaytracingShader(String filePath);

		[[nodiscard]] RaytracingShader* GetRaytracingShader(const Handle<RaytracingShader>& handle)noexcept;

		[[nodiscard]] Handle<ComputeShader> GetOrCreateComputeShader(String filePath, String entryPoint = String("main"));

		[[nodiscard]] ComputeShader* GetComputeShader(const Handle<ComputeShader>& handle)noexcept;

		void Clear()noexcept;

	private:
		ArtMap<Key, Handle<VertexShader>> vertexShaderCache_;

		ArtMap<Key, Handle<HullShader>> hullShaderCache_;

		ArtMap<Key, Handle<DomainShader>> domainShaderCache_;

		ArtMap<Key, Handle<GeometryShader>> geometryShaderCache_;

		ArtMap<Key, Handle<PixelShader>> pixelShaderCache_;

		ArtMap<Key, Handle<AmplificationShader>> amplificationShaderCache_;

		ArtMap<Key, Handle<MeshShader>> meshShaderCache_;

		ArtMap<Key, Handle<RaytracingShader>> raytracingShaderCache_;

		ArtMap<Key, Handle<ComputeShader>> computeShaderCache_;
	};
}
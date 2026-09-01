#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/ModelLoader.h>
#include <GraphicsEngine/Model/Material/MaterialLoader.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	Handle<Crister> ModelResource::Load(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		Asset* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<Crister>::null();
		}

		AxisConvention axisConvention = cache.ReadAxisConvention(assetId);
		Handle<Crister> handle = loader.modelLoader_->Load(loader, device, cmdQueue, heap, bc7Shader, asset->fullpath_, axisConvention);
		if (handle.empty())
		{
			return Handle<Crister>::null();
		}

		assetHandleMap_.insert({ assetId, handle });

		/// [EN] Import-time material extraction: write a ".material" sibling
		///      per slot the first time this model is loaded (existing files
		///      are kept). They become AssetType::Material assets on the next
		///      ResourceCache scan and back the Mesh component's material slots.
		/// [JP] インポート時のマテリアル抽出: このモデルを初めてロードした時に
		///      スロットごとの ".material" 兄弟ファイルを書き出す（既存は
		///      維持）。次回の ResourceCache スキャンで AssetType::Material
		///      アセットになり、Mesh コンポーネントのマテリアルスロットの
		///      裏付けになる。
		if (Crister* crister = loader.modelLoader_->Get(handle))
		{
			const DynamicArray<Surface>& materials = crister->Surfaces();
			std::filesystem::path modelPath(asset->fullpath_.c_str());
			std::filesystem::path directory = modelPath.parent_path() / (modelPath.stem().string() + ".Materials");
			std::error_code error;
			std::filesystem::create_directories(directory, error);

			std::set<std::string> usedNames;
			for (Size materialIndex = 0; materialIndex < materials.size(); materialIndex++)
			{
				const Surface& material = materials[materialIndex];

				std::string stem = material.name_.empty() ? ("Material_" + std::to_string(materialIndex)) : material.name_;
				if (usedNames.contains(stem))
				{
					stem += "_" + std::to_string(materialIndex);
				}
				usedNames.insert(stem);

				std::filesystem::path filePath = directory / (stem + ".material");
				if (std::filesystem::exists(filePath))
				{
					continue;
				}
				loader.materialLoader_->Save(material, String(filePath.string()));
			}
		}

		return handle;
	}

	Bool ModelResource::GenerateCollision(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader, ResourceCache& cache, Uint32 assetId, MeshCollisionDetail detail)
	{
		Asset* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return false;
		}

		/// [EN] The bake reads CPU-resident cluster data off the Crister, so the model has to be loaded first - Load() is a no-op when it already is.
		/// [JP] ベイクは Crister の CPU 常駐クラスタデータを読むため、先にモデルをロードしておく必要がある - 既にロード済みなら Load() は何もしない。
		Handle<Crister> handle = Load(loader, device, cmdQueue, heap, bc7Shader, cache, assetId);
		Crister* crister = loader.modelLoader_->Get(handle);
		if (!crister)
		{
			return false;
		}

		std::filesystem::path modelPath(asset->fullpath_.c_str());
		std::filesystem::path collisionPath = modelPath.parent_path() / (modelPath.stem().string() + ((detail == MeshCollisionDetail::Exact) ? ".exact.collision" : ".proxy.collision"));

		return loader.meshCollisionLoader_->Bake(*crister, detail, String(collisionPath.string()));
	}

	Bool ModelResource::GenerateMaterial(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader, ResourceCache& cache, Uint32 assetId, Bool overwrite)
	{
		Asset* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return false;
		}

		Handle<Crister> handle = Load(loader, device, cmdQueue, heap, bc7Shader, cache, assetId);
		Crister* crister = loader.modelLoader_->Get(handle);
		if (!crister)
		{
			return false;
		}

		const DynamicArray<Surface>& materials = crister->Surfaces();
		if (materials.empty())
		{
			return false;
		}

		std::filesystem::path modelPath(asset->fullpath_.c_str());
		std::filesystem::path directory = modelPath.parent_path() / (modelPath.stem().string() + ".Materials");
		std::error_code error;
		std::filesystem::create_directories(directory, error);

		std::set<std::string> usedNames;
		Bool allWritten = true;
		for (Size materialIndex = 0; materialIndex < materials.size(); materialIndex++)
		{
			const Surface& material = materials[materialIndex];

			std::string stem = material.name_.empty() ? ("Material_" + std::to_string(materialIndex)) : material.name_;
			if (usedNames.contains(stem))
			{
				stem += "_" + std::to_string(materialIndex);
			}
			usedNames.insert(stem);

			std::filesystem::path filePath = directory / (stem + ".material");
			if (!overwrite && std::filesystem::exists(filePath))
			{
				continue;
			}
			if (!loader.materialLoader_->Save(material, String(filePath.string())))
			{
				allWritten = false;
			}
		}

		return allWritten;
	}

	Handle<Crister> ModelResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<Crister>::null();
		}
		return assetHandleMap_.at(assetId);
	}

	Crister* ModelResource::Resolve(LoaderSystem& loader, const Handle<Crister>& handle)
	{
		return loader.modelLoader_->Get(handle);
	}

	Bool ModelResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void ModelResource::Unload(LoaderSystem& loader, Uint32 assetId, BindlessHeap* heap)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<Crister> handle = assetHandleMap_.at(assetId);
		loader.modelLoader_->Clear(handle, heap);
		assetHandleMap_.erase(assetId);
	}
}

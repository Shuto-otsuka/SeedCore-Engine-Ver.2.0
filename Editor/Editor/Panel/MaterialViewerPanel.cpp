#include <Editor/Editor/Panel/MaterialViewerPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiCommon.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <External/ImGui/Include/imgui_internal.h>
#include <GraphicsEngine/Model/Material/Material.h>
#include <GraphicsEngine/Model/Material/MaterialLoader.h>
#include <GraphicsEngine/Model/Material/MaterialResource.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/Mesh.h>
#include <GraphicsEngine/Graphics.h>
#include <GraphicsEngine/Camera/PreviewCamera.h>
#include <GraphicsEngine/Camera/PreviewCameraController.h>
#include <FoundationEngine/Input/InputSystem.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	namespace
	{
		std::string SlotLabel(ResourceCache* resource, Uint32 assetId, Size slot)
		{
			if (assetId == 0)
			{
				return "[" + std::to_string(slot) + "] (未割り当て)";
			}
			Asset* asset = resource->GetAsset(assetId);
			std::string name = asset ? std::filesystem::path(asset->path_.c_str()).stem().string() : std::to_string(assetId);
			return "[" + std::to_string(slot) + "] " + name;
		}
	}

	MaterialViewerPanel::MaterialViewerPanel(EditorContext& context) : context_(context)
	{
		surfaceNewNameBuffer_.resize(128);
	}

	MaterialViewerPanel::~MaterialViewerPanel() = default;

	void MaterialViewerPanel::Open()
	{
		show_ = true;
		ImGui::SetWindowFocus("マテリアルビューア");
	}

	void MaterialViewerPanel::SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle)
	{
		previewHandle_ = previewHandle;
	}

	Bool MaterialViewerPanel::IsFocused()const
	{
		return isFocused_;
	}

	void MaterialViewerPanel::EnsureEditingSurface()
	{
		Uint32 surfaceAssetId = context_.materialPreviewContext_.previewSurfaceAssetId_;
		if (surfaceAssetId == 0 || editingSurfaceAssetId_ == surfaceAssetId)
		{
			return;
		}

		LoaderSystem* loader = context_.worldContext_.loader_;
		ResourceCache* cache = context_.worldContext_.resource_;
		MaterialResource* materialResource = cache->GetMaterialResource();

		Handle<Surface> handle = materialResource->GetHandle(surfaceAssetId);
		if (handle.empty())
		{
			handle = materialResource->Load(*loader, *cache, surfaceAssetId);
		}
		Surface* loaded = loader->materialLoader_->Get(handle);
		if (!loaded)
		{
			return;
		}

		editingSurface_ = MakePtr<Surface>(*loaded);
		editingSurfaceAssetId_ = surfaceAssetId;

		Asset* asset = cache->GetAsset(surfaceAssetId);
		surfaceNewNameBuffer_ = asset ? std::filesystem::path(asset->fullpath_.c_str()).stem().string() : std::string();
		surfaceNewNameBuffer_.resize(128);
	}

	void MaterialViewerPanel::Draw()
	{
		context_.materialPreviewContext_.previewActive_ = false;
		context_.materialPreviewContext_.previewSurfaceAssetId_ = 0;
		isFocused_ = false;

		if (!show_)
		{
			return;
		}

		Actor actor = context_.selectionContext_.selectedActor_;
		target_ = actor ? actor.GetComponent<Material>() : nullptr;

		ImGui::DockBuilderDockWindow("マテリアルビューア", context_.graphicsContext_.imgui_->GetDockSpaceID());
		ImGui::SetNextWindowSize(ImVec2(960, 720), ImGuiCond_FirstUseEver);

		isFocused_ = ImGui::Begin("マテリアルビューア", &show_);
		if (isFocused_)
		{
			const Mesh* mesh = actor ? actor.GetComponent<Mesh>() : nullptr;

			if (!target_ || !mesh || mesh->meshID_ == 0)
			{
				ImGui::TextDisabled("Material コンポーネントとメッシュを持つアクターを選択してください");
			}
			else if (target_->materialIDs_.empty())
			{
				ImGui::TextDisabled("マテリアルスロットがありません");
			}
			else
			{
				if (selectedSlot_ >= target_->materialIDs_.size())
				{
					selectedSlot_ = 0;
				}

				ImGui::SetNextItemWidth(240.0f);
				if (ImGui::BeginCombo("マテリアル", SlotLabel(context_.worldContext_.resource_, target_->materialIDs_[selectedSlot_], selectedSlot_).c_str()))
				{
					for (Size slot = 0; slot < target_->materialIDs_.size(); slot++)
					{
						if (ImGui::Selectable(SlotLabel(context_.worldContext_.resource_, target_->materialIDs_[slot], slot).c_str(), selectedSlot_ == slot))
						{
							selectedSlot_ = slot;
						}
					}
					ImGui::EndCombo();
				}

				context_.materialPreviewContext_.previewActive_ = true;
				context_.materialPreviewContext_.previewMeshAssetId_ = mesh->meshID_;
				context_.materialPreviewContext_.previewSurfaceAssetId_ = target_->materialIDs_[selectedSlot_];

				ImGui::Spacing();

				ImVec2 previewSize = ImGui::GetContentRegionAvail();
				previewSize.y = Max(previewSize.y, 100.0f);

				if (context_.cameraContext_.materialCamera_)
				{
					context_.cameraContext_.materialCamera_->Resize(previewSize.x, previewSize.y);
				}

				ImGui::Image(ImTextureID(previewHandle_.ptr), previewSize);

				Bool orbitHeld = InputSystem::MouseState(InputSystem::MouseButton::Left, InputSystem::IsPressed);
				Bool panHeld = InputSystem::MouseState(InputSystem::MouseButton::Middle, InputSystem::IsPressed);

				if (ImGui::IsItemHovered() && context_.cameraContext_.materialCamera_ && context_.cameraContext_.materialCameraController_)
				{
					if ((orbitHeld || panHeld) && !InputSystem::IsMouseCaptured())
					{
						InputSystem::BeginMouseCapture();
					}
					context_.cameraContext_.materialCameraController_->Update(*context_.cameraContext_.materialCamera_, ImGui::GetIO().DeltaTime);
				}

				if (!orbitHeld && !panHeld && InputSystem::IsMouseCaptured())
				{
					InputSystem::EndMouseCapture();
				}
			}
		}
		ImGui::End();
	}

	void MaterialViewerPanel::DrawDetails()
	{
		if (!target_ || target_->materialIDs_.empty() || selectedSlot_ >= target_->materialIDs_.size())
		{
			ImGui::TextDisabled("マテリアルビューアでスロットを選択してください");
			return;
		}

		Uint32 surfaceAssetId = target_->materialIDs_[selectedSlot_];
		if (surfaceAssetId == 0)
		{
			ImGui::TextDisabled("このスロットは未割り当てです（Crister 内蔵を使用）");
			return;
		}

		EnsureEditingSurface();
		if (!editingSurface_)
		{
			ImGui::TextDisabled("マテリアルを読み込めません");
			return;
		}

		LoaderSystem* loader = context_.worldContext_.loader_;
		ResourceCache* cache = context_.worldContext_.resource_;
		MaterialResource* materialResource = cache->GetMaterialResource();
		Surface& surface = *editingSurface_;

		if (ImGui::Button("上書き保存"))
		{
			if (Asset* asset = cache->GetAsset(surfaceAssetId))
			{
				loader->materialLoader_->Save(surface, asset->fullpath_);
				materialResource->Unload(*loader, surfaceAssetId);
				materialResource->Load(*loader, *cache, surfaceAssetId);
			}
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150.0f);
		ImGui::InputText("##surfaceNewName", surfaceNewNameBuffer_.data(), surfaceNewNameBuffer_.capacity());
		ImGui::SameLine();
		if (ImGui::Button("新規保存"))
		{
			std::string name(surfaceNewNameBuffer_.c_str());
			Asset* asset = cache->GetAsset(surfaceAssetId);
			if (asset && !name.empty())
			{
				std::filesystem::path newPath = std::filesystem::path(asset->fullpath_.c_str()).parent_path() / (name + ".material");
				loader->materialLoader_->Save(surface, String(newPath.string()));
				D3D12Context* d3d12Context = context_.graphicsContext_.graphics_->GetContext();
				cache->Reload(*loader, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), context_.graphicsContext_.graphics_->GetBC7CompressShader());
			}
		}

		ImGui::Separator();

		const Char* shadingModels = "PBR\0Unlit\0Phong\0Toon\0Lambert\0";
		Int shadingModel = static_cast<Int>(surface.shadingModel_);
		if (ImGui::Combo("シェーディングモデル", &shadingModel, shadingModels))
		{
			surface.shadingModel_ = static_cast<ShadingModel>(shadingModel);
		}

		Bool doubleSided = surface.doubleSided_ != 0;
		if (ImGui::Checkbox("両面描画", &doubleSided))
		{
			surface.doubleSided_ = doubleSided ? 1 : 0;
		}

		const Char* alphaModes = "OPAQUE\0MASK\0BLEND\0";
		ImGui::Combo("アルファモード", &surface.alphaMode_, alphaModes);
		if (surface.alphaMode_ == 1)
		{
			ImGui::SliderFloat("アルファカットオフ", &surface.alphaCutoff_, 0.0f, 1.0f);
		}

		ImGui::ColorEdit4("ベースカラー", &surface.baseColor_.x);
		ImGui::SliderFloat("メタリック", &surface.metallic_, 0.0f, 1.0f);
		ImGui::SliderFloat("ラフネス", &surface.roughness_, 0.0f, 1.0f);
		ImGui::ColorEdit3("エミッシブ", surface.emissiveFactor_);

		if (ImGui::CollapsingHeader("KHR_materials"))
		{
			KHR& khr = surface.khr_;

			ImGui::DragFloat("emissive_strength", &khr.emissiveStrength_.emissiveStrength_, 0.01f, 0.0f, 100.0f);
			ImGui::DragFloat("ior", &khr.ior_.ior_, 0.01f, 1.0f, 3.0f);

			ImGui::SeparatorText("specular");
			ImGui::SliderFloat("specular_factor", &khr.specular_.specularFactor_, 0.0f, 1.0f);
			ImGui::ColorEdit3("specular_color", khr.specular_.specularColorFactor_);

			ImGui::SeparatorText("clearcoat");
			ImGui::SliderFloat("clearcoat_factor", &khr.clearCoat_.clearCoatFactor_, 0.0f, 1.0f);
			ImGui::SliderFloat("clearcoat_roughness", &khr.clearCoat_.clearCoatRoughnessFactor_, 0.0f, 1.0f);

			ImGui::SeparatorText("transmission");
			ImGui::SliderFloat("transmission_factor", &khr.transmission_.transmissionFactor_, 0.0f, 1.0f);

			ImGui::SeparatorText("volume");
			ImGui::DragFloat("thickness_factor", &khr.volume_.thicknessFactor_, 0.001f, 0.0f, 10.0f);
			ImGui::DragFloat("attenuation_distance", &khr.volume_.attenuationDistance_, 0.01f, 0.0f, FLT_MAX);
			ImGui::ColorEdit3("attenuation_color", khr.volume_.attenuationColor_);

			ImGui::SeparatorText("sheen");
			ImGui::ColorEdit3("sheen_color", khr.sheen_.sheenColorFactor_);
			ImGui::SliderFloat("sheen_roughness", &khr.sheen_.sheenRoughnessFactor_, 0.0f, 1.0f);

			ImGui::SeparatorText("iridescence");
			ImGui::SliderFloat("iridescence_factor", &khr.iridescence_.iridescenceFactor_, 0.0f, 1.0f);
			ImGui::DragFloat("iridescence_ior", &khr.iridescence_.iridescenceIor_, 0.01f, 1.0f, 3.0f);
			ImGui::DragFloat("iridescence_thickness_min", &khr.iridescence_.iridescenceThicknessMinimum_, 1.0f, 0.0f, 2000.0f);
			ImGui::DragFloat("iridescence_thickness_max", &khr.iridescence_.iridescenceThicknessMaximum_, 1.0f, 0.0f, 2000.0f);

			ImGui::SeparatorText("anisotropy");
			ImGui::SliderFloat("anisotropy_strength", &khr.anisotropy_.anisotropyStrength_, 0.0f, 1.0f);
			ImGui::SliderAngle("anisotropy_rotation", &khr.anisotropy_.anisotropyRotation_);

			ImGui::SeparatorText("unlit");
			Bool unlit = khr.unlit_.unlit_ != 0;
			if (ImGui::Checkbox("unlit", &unlit))
			{
				khr.unlit_.unlit_ = unlit ? 1 : 0;
			}
		}
	}
}

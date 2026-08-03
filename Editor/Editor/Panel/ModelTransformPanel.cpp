#include <Editor/Editor/Panel/ModelTransformPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiCommon.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/Mesh.h>
#include <GraphicsEngine/Camera/PreviewCamera.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>

namespace SeedCore
{
	namespace
	{
		constexpr Float inspectorColumnWidthRatio = 0.32f;

		const Char* SignedAxisLabel(SignedAxis axis)
		{
			switch (axis)
			{
			case SignedAxis::PositiveX:
				return "+X";
			case SignedAxis::NegativeX:
				return "-X";
			case SignedAxis::PositiveY:
				return "+Y";
			case SignedAxis::NegativeY:
				return "-Y";
			case SignedAxis::PositiveZ:
				return "+Z";
			case SignedAxis::NegativeZ:
			default:
				return "-Z";
			}
		}

		Bool DrawAxisCombo(const Char* label, SignedAxis& axis)
		{
			constexpr SignedAxis options[] = { SignedAxis::PositiveX, SignedAxis::NegativeX, SignedAxis::PositiveY, SignedAxis::NegativeY, SignedAxis::PositiveZ, SignedAxis::NegativeZ };

			Bool changed = false;
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::BeginCombo(label, SignedAxisLabel(axis)))
			{
				for (SignedAxis option : options)
				{
					Bool selected = (axis == option);
					if (ImGui::Selectable(SignedAxisLabel(option), selected))
					{
						axis = option;
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
			return changed;
		}
	}

	ModelTransformPanel::ModelTransformPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void ModelTransformPanel::Open()
	{
		show_ = true;
	}

	void ModelTransformPanel::SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle)
	{
		previewHandle_ = previewHandle;
	}

	void ModelTransformPanel::Draw()
	{
		if (!show_)
		{
			context_.previewActive_ = false;
			return;
		}

		context_.previewActive_ = false;

		ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("モデル変換", &show_))
		{
			const Mesh* mesh = context_.selectedActor_ ? context_.selectedActor_->GetComponent<Mesh>() : nullptr;

			if (!mesh || mesh->meshID_ == 0)
			{
				ImGui::TextDisabled("対象のメッシュがありません");
			}
			else
			{
				if (mesh->meshID_ != targetMeshAssetId_)
				{
					targetMeshAssetId_ = mesh->meshID_;
					editConvention_ = context_.resource_->ReadAxisConvention(targetMeshAssetId_);
				}

				ModelResource* modelResource = context_.resource_->GetModelResource();
				Handle<Crister> handle = modelResource->GetHandle(targetMeshAssetId_);
				Crister* crister = handle.empty() ? nullptr : modelResource->Resolve(*context_.loader_, handle);

				if (!crister)
				{
					ImGui::TextDisabled("モデルを読み込み中です...");
				}
				else
				{
					Float inspectorWidth = ImGui::GetContentRegionAvail().x * inspectorColumnWidthRatio;

					ImGui::BeginChild("##previewColumn", ImVec2(-inspectorWidth, 0.0f), true);
					{
						DrawPreview();
					}
					ImGui::EndChild();

					ImGui::SameLine();

					ImGui::BeginChild("##axisInspectorColumn", ImVec2(0.0f, 0.0f), true);
					{
						DrawAxisInspector(targetMeshAssetId_);
					}
					ImGui::EndChild();
				}
			}
		}
		ImGui::End();
	}

	void ModelTransformPanel::DrawPreview()
	{
		context_.previewActive_ = true;
		context_.previewMeshAssetId_ = targetMeshAssetId_;
		context_.previewAnimationAssetId_ = 0;
		context_.previewTime_ = 0.0f;

		ImVec2 previewSize = ImGui::GetContentRegionAvail();
		if (context_.previewCamera_)
		{
			context_.previewCamera_->Resize(previewSize.x, previewSize.y);
		}

		ImGui::Image(ImTextureID(previewHandle_.ptr), previewSize);
	}

	void ModelTransformPanel::DrawAxisInspector(Uint32 assetId)
	{
		ImGui::TextDisabled("軸コンベンション");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Up");
		DrawAxisCombo("##up", editConvention_.up_);

		ImGui::Text("Right");
		DrawAxisCombo("##right", editConvention_.right_);

		ImGui::Text("Forward");
		DrawAxisCombo("##forward", editConvention_.forward_);

		ResolvedAxisConvention resolved = ResolvedAxisConvention::Resolve(editConvention_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (!resolved.valid_)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "無効な軸の組み合わせです（同じ軸を複数回選択しています）");
		}
		else
		{
			ImGui::Text("利き手 (自動判定): %s", resolved.isRightHanded_ ? "右手系" : "左手系");
		}

		ImGui::Spacing();
		ImGui::Text("巻き順");
		Int windingOverride = static_cast<Int>(editConvention_.windingOverride_);
		if (ImGui::RadioButton("自動", windingOverride == static_cast<Int>(WindingOverride::Auto)))
		{
			editConvention_.windingOverride_ = WindingOverride::Auto;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("そのまま", windingOverride == static_cast<Int>(WindingOverride::AsIs)))
		{
			editConvention_.windingOverride_ = WindingOverride::AsIs;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("反転", windingOverride == static_cast<Int>(WindingOverride::Flip)))
		{
			editConvention_.windingOverride_ = WindingOverride::Flip;
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginDisabled(!resolved.valid_);
		if (ImGui::Button("適用"))
		{
			ApplyConversion(assetId);
		}
		ImGui::EndDisabled();
	}

	void ModelTransformPanel::ApplyConversion(Uint32 assetId)
	{
		Asset* asset = context_.resource_->GetAsset(assetId);
		if (!asset)
		{
			return;
		}

		std::filesystem::path path(asset->fullpath_.c_str());
		Bool isSourceAsset = (path.extension() == ".gltf" || path.extension() == ".glb");

		ModelResource* modelResource = context_.resource_->GetModelResource();
		BindlessHeap* heap = context_.resource_->Heap();

		if (isSourceAsset)
		{
			context_.resource_->WriteAssetMeta(assetId, editConvention_);
			modelResource->Unload(*context_.loader_, assetId, heap);
			modelResource->Load(*context_.loader_, context_.device_, context_.cmdQueue_, heap, *context_.bc7Shader_, *context_.resource_, assetId);
		}
		else
		{
			AxisConvention previousConvention = context_.resource_->ReadAxisConvention(assetId);
			ResolvedAxisConvention oldResolved = ResolvedAxisConvention::Resolve(previousConvention);
			ResolvedAxisConvention newResolved = ResolvedAxisConvention::Resolve(editConvention_);
			Matrix deltaBasis = oldResolved.basis_.Transpose() * newResolved.basis_;

			Handle<Crister> handle = modelResource->GetHandle(assetId);
			Crister* crister = handle.empty() ? nullptr : modelResource->Resolve(*context_.loader_, handle);
			if (!crister || !crister->ApplyAxisConversion(deltaBasis, newResolved.flipWinding_, path))
			{
				return;
			}

			context_.resource_->WriteAssetMeta(assetId, editConvention_);
			modelResource->Unload(*context_.loader_, assetId, heap);
			modelResource->Load(*context_.loader_, context_.device_, context_.cmdQueue_, heap, *context_.bc7Shader_, *context_.resource_, assetId);
		}
	}
}

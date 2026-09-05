#include <Editor/Editor/Panel/SkeletonControllerPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiCommon.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <External/ImGui/Include/imgui_internal.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/Mesh.h>
#include <GraphicsEngine/Camera/PreviewCamera.h>
#include <GraphicsEngine/Camera/PreviewCameraController.h>
#include <FoundationEngine/Input/InputSystem.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	SkeletonControllerPanel::SkeletonControllerPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void SkeletonControllerPanel::Open()
	{
		show_ = true;
		ImGui::SetWindowFocus("スケルトンコントローラー");
	}

	void SkeletonControllerPanel::SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle)
	{
		previewHandle_ = previewHandle;
	}

	Bool SkeletonControllerPanel::IsFocused()const
	{
		return isFocused_;
	}

	void SkeletonControllerPanel::Draw()
	{
		context_.skeletonControllerPreviewContext_.previewActive_ = false;
		context_.skeletonControllerPreviewContext_.selectedNodeIndex_ = selectedNodeIndex_;
		currentCrister_ = nullptr;
		isFocused_ = false;

		if (!show_)
		{
			return;
		}

		ImGui::DockBuilderDockWindow("スケルトンコントローラー", context_.graphicsContext_.imgui_->GetDockSpaceID());
		ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_FirstUseEver);

		isFocused_ = ImGui::Begin("スケルトンコントローラー", &show_);
		if (isFocused_)
		{
			const Mesh* mesh = context_.selectionContext_.selectedActor_ ? context_.selectionContext_.selectedActor_.GetComponent<Mesh>() : nullptr;

			if (!mesh || mesh->meshID_ == 0)
			{
				ImGui::TextDisabled("スケルタルメッシュを持つ Actor を選択してください");
			}
			else
			{
				ModelResource* modelResource = context_.worldContext_.resource_->GetModelResource();
				Handle<Crister> handle = modelResource->GetHandle(mesh->meshID_);
				currentCrister_ = handle.empty() ? nullptr : modelResource->Resolve(*context_.worldContext_.loader_, handle);

				if (!currentCrister_)
				{
					ImGui::TextDisabled("モデルを読み込み中です...");
				}
				else
				{
					Float boneTreeWidth = ImGui::GetContentRegionAvail().x * boneTreeColumnWidthRatio_;

					ImGui::BeginChild("##boneTreeColumn", ImVec2(boneTreeWidth, 0.0f), true);
					{
						DrawBoneList(*currentCrister_);
					}
					ImGui::EndChild();

					ImGui::SameLine();

					ImGui::BeginChild("##previewColumn", ImVec2(0.0f, 0.0f), true);
					{
						DrawPreview();
					}
					ImGui::EndChild();
				}
			}
		}
		ImGui::End();
	}

	void SkeletonControllerPanel::DrawBoneList(const Crister& crister)
	{
		if (ImGui::Selectable("Rig", selectedNodeIndex_ < 0))
		{
			selectedNodeIndex_ = -1;
		}

		ImGui::Separator();

		const DynamicArray<Node>& nodes = crister.Nodes();

		for (const Skin& skin : crister.Skins())
		{
			for (Int jointIndex : skin.joints_)
			{
				if (jointIndex < 0 || static_cast<Size>(jointIndex) >= nodes.size())
				{
					continue;
				}

				const Char* label = nodes[static_cast<Size>(jointIndex)].name_.c_str();
				if (ImGui::Selectable(label, selectedNodeIndex_ == jointIndex))
				{
					selectedNodeIndex_ = jointIndex;
				}
			}
		}
	}

	void SkeletonControllerPanel::DrawPreview()
	{
		const Mesh* mesh = context_.selectionContext_.selectedActor_.GetComponent<Mesh>();

		context_.skeletonControllerPreviewContext_.previewActive_ = true;
		context_.skeletonControllerPreviewContext_.previewMeshAssetId_ = mesh->meshID_;
		context_.skeletonControllerPreviewContext_.previewWorldMatrix_ = Matrix::Identity;

		ImVec2 previewSize = ImGui::GetContentRegionAvail();
		previewSize.y = Max(previewSize.y, 100.0f);

		if (context_.cameraContext_.skeletonControllerCamera_)
		{
			context_.cameraContext_.skeletonControllerCamera_->Resize(previewSize.x, previewSize.y);
		}

		ImVec2 imagePosition = ImGui::GetCursorScreenPos();
		ImGui::Image(ImTextureID(previewHandle_.ptr), previewSize);

		/// [EN] Orbit-style preview navigation: left-click drag orbits around
		///      Focus(), middle-click drag pans, wheel dollies - see
		///      PreviewCameraController. Gated on hovering the image.
		/// [JP] オービット式のプレビュー操作: 左クリックドラッグでFocus()を
		///      中心に回転、中クリックドラッグでパン、ホイールでドリー -
		///      PreviewCameraController参照。画像自体のホバーでゲートする。
		Bool orbitHeld = InputSystem::MouseState(InputSystem::MouseButton::Left, InputSystem::IsPressed);
		Bool panHeld = InputSystem::MouseState(InputSystem::MouseButton::Middle, InputSystem::IsPressed);

		if (ImGui::IsItemHovered() && context_.cameraContext_.skeletonControllerCamera_ && context_.cameraContext_.skeletonControllerCameraController_)
		{
			if ((orbitHeld || panHeld) && !InputSystem::IsMouseCaptured())
			{
				InputSystem::BeginMouseCapture();
			}

			context_.cameraContext_.skeletonControllerCameraController_->Update(*context_.cameraContext_.skeletonControllerCamera_, ImGui::GetIO().DeltaTime);
		}

		if (!orbitHeld && !panHeld && InputSystem::IsMouseCaptured())
		{
			InputSystem::EndMouseCapture();
		}

		/// [EN] Ray-vs-joint picking: a plain click (not a drag past the
		///      threshold, so it doesn't fight the orbit above) unprojects
		///      the mouse through the preview camera's inverse
		///      view-projection at an arbitrary NDC depth, then selects
		///      whichever joint's position lies closest to that ray.
		/// [JP] レイによるジョイント選択: (上のオービットと競合しないよう)
		///      閾値を超えないただのクリックの場合のみ、プレビューカメラの
		///      逆ビュープロジェクションで任意のNDC深度からマウス位置を
		///      ワールド空間へ逆射影し、そのレイに最も近いジョイントを選択する。
		if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f) && context_.cameraContext_.skeletonControllerCamera_ && currentCrister_)
		{
			Vector2 mousePosInImage(ImGui::GetMousePos().x - imagePosition.x, ImGui::GetMousePos().y - imagePosition.y);
			Float ndcX = (mousePosInImage.x / previewSize.x) * 2.0f - 1.0f;
			Float ndcY = 1.0f - (mousePosInImage.y / previewSize.y) * 2.0f;

			Matrix inverseViewProjection = context_.cameraContext_.skeletonControllerCamera_->InverseViewProjection();
			Vector4 unprojected = Vector4::Transform(Vector4(ndcX, ndcY, 0.5f, 1.0f), inverseViewProjection);
			Vector3 worldPoint = Vector3(unprojected.x, unprojected.y, unprojected.z) / unprojected.w;

			Vector3 rayOrigin = context_.cameraContext_.skeletonControllerCamera_->Eye();
			Vector3 rayDirection = worldPoint - rayOrigin;
			rayDirection.Normalize();

			Int closestJointIndex = -1;
			Float closestDistance = 0.08f;

			const DynamicArray<Node>& nodes = currentCrister_->Nodes();
			for (const Skin& skin : currentCrister_->Skins())
			{
				for (Int jointIndex : skin.joints_)
				{
					if (jointIndex < 0 || static_cast<Size>(jointIndex) >= nodes.size())
					{
						continue;
					}

					Vector3 jointPosition = nodes[static_cast<Size>(jointIndex)].globalTransform_.Translation();
					Vector3 toJoint = jointPosition - rayOrigin;
					Float projectedDistance = toJoint.Dot(rayDirection);
					if (projectedDistance < 0.0f)
					{
						continue;
					}

					Vector3 closestPointOnRay = rayOrigin + rayDirection * projectedDistance;
					Float perpendicularDistance = Vector3::Distance(closestPointOnRay, jointPosition);
					if (perpendicularDistance < closestDistance)
					{
						closestDistance = perpendicularDistance;
						closestJointIndex = jointIndex;
					}
				}
			}

			if (closestJointIndex >= 0)
			{
				selectedNodeIndex_ = closestJointIndex;
			}
		}
	}

	void SkeletonControllerPanel::DrawDetails()
	{
		if (!currentCrister_)
		{
			ImGui::TextDisabled("プレビュー中のモデルがありません");
			return;
		}

		const DynamicArray<Node>& nodes = currentCrister_->Nodes();

		if (selectedNodeIndex_ < 0 || static_cast<Size>(selectedNodeIndex_) >= nodes.size())
		{
			selectedNodeIndex_ = -1;

			ImGui::TextDisabled("Rig");
			ImGui::Separator();
			ImGui::Text("ノード数: %zu", nodes.size());
			ImGui::Text("スキン数: %zu", currentCrister_->Skins().size());
			return;
		}

		const Node& node = nodes[static_cast<Size>(selectedNodeIndex_)];

		ImGui::TextDisabled("Bone");
		ImGui::Separator();
		ImGui::Text("名前: %s", node.name_.c_str());

		if (node.parentIndex_ >= 0 && static_cast<Size>(node.parentIndex_) < nodes.size())
		{
			ImGui::Text("親: %s", nodes[static_cast<Size>(node.parentIndex_)].name_.c_str());
		}
		else
		{
			ImGui::Text("親: なし");
		}

		Vector3 position = node.globalTransform_.Translation();
		ImGui::Text("位置: (%.3f, %.3f, %.3f)", position.x, position.y, position.z);
	}
}

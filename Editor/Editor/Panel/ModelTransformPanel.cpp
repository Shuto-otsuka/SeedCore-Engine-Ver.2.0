#include <Editor/Editor/Panel/ModelTransformPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiCommon.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <External/ImGui/Include/imgui_internal.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <GraphicsEngine/Graphics.h>
#include <GraphicsEngine/Model/Mesh.h>
#include <GraphicsEngine/Camera/PreviewCamera.h>
#include <GraphicsEngine/Camera/PreviewCameraController.h>
#include <FoundationEngine/Input/InputSystem.h>
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

	void ModelTransformPanel::Open(Uint32 assetId)
	{
		show_ = true;
		SetTarget(assetId);

		/// [EN] Prime the selection edge-detector with whatever is selected right now, so this asset target is not immediately overridden by an unchanged scene selection on the next Draw.
		/// [JP] 選択のエッジ検出を「今選択されているもの」で初期化し、次の Draw で変化していないシーン選択にこのアセットターゲットが即座に上書きされないようにする。
		const Mesh* mesh = context_.selectionContext_.selectedActor_ ? context_.selectionContext_.selectedActor_->GetComponent<Mesh>() : nullptr;
		lastSelectionMeshId_ = (mesh && mesh->meshID_ != 0) ? mesh->meshID_ : 0;
	}

	void ModelTransformPanel::SetTarget(Uint32 assetId)
	{
		targetMeshAssetId_ = assetId;
		editConvention_ = context_.worldContext_.resource_->ReadAxisConvention(assetId);
		baseTransformPosition_ = Vector3(0.0f, 0.0f, 0.0f);
		baseTransformRotation_ = Vector3(0.0f, 0.0f, 0.0f);
		baseTransformScale_ = Vector3(1.0f, 1.0f, 1.0f);
		baseTransformPivot_ = Vector3(0.0f, 0.0f, 0.0f);
	}

	void ModelTransformPanel::SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle)
	{
		previewHandle_ = previewHandle;
	}

	void ModelTransformPanel::Draw()
	{
		if (!show_)
		{
			context_.modelTransformPreviewContext_.previewActive_ = false;
			context_.modelTransformPreviewContext_.previewWorldMatrix_ = Matrix::Identity;
			return;
		}

		context_.modelTransformPreviewContext_.previewActive_ = false;
		context_.modelTransformPreviewContext_.previewWorldMatrix_ = Matrix::Identity;

		ImGui::DockBuilderDockWindow("モデル変換", context_.graphicsContext_.imgui_->GetDockSpaceID());
		ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("モデル変換", &show_))
		{
			const Mesh* mesh = context_.selectionContext_.selectedActor_ ? context_.selectionContext_.selectedActor_->GetComponent<Mesh>() : nullptr;
			Uint32 selectionMeshId = (mesh && mesh->meshID_ != 0) ? mesh->meshID_ : 0;

			/// [EN] Follow the scene selection only when it actually changes, not merely when it differs from the current target - otherwise opening the panel on a content-drawer asset while an actor is selected would snap straight back to that actor's mesh.
			/// [JP] シーン選択に追従するのは選択が実際に変わったときだけで、現在のターゲットと違うというだけでは追従しない - そうしないと、Actor を選択したままコンテンツドロワーのアセットでパネルを開いた瞬間に、その Actor のメッシュへ戻ってしまう。
			if (selectionMeshId != 0 && selectionMeshId != lastSelectionMeshId_)
			{
				SetTarget(selectionMeshId);
			}
			lastSelectionMeshId_ = selectionMeshId;

			if (targetMeshAssetId_ == 0)
			{
				ImGui::TextDisabled("対象のメッシュがありません");
			}
			else
			{
				ModelResource* modelResource = context_.worldContext_.resource_->GetModelResource();
				Handle<Crister> handle = modelResource->GetHandle(targetMeshAssetId_);
				Crister* crister = handle.empty() ? nullptr : modelResource->Resolve(*context_.worldContext_.loader_, handle);

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
						DrawBaseTransformInspector(targetMeshAssetId_);
					}
					ImGui::EndChild();
				}
			}
		}
		ImGui::End();
	}

	void ModelTransformPanel::DrawPreview()
	{
		context_.modelTransformPreviewContext_.previewActive_ = true;
		context_.modelTransformPreviewContext_.previewMeshAssetId_ = targetMeshAssetId_;

		/// [EN] Mirrors Crister::ApplyTransformConversion's fullTransform exactly,
		///      so the preview is a WYSIWYG of what 適用 will bake.
		/// [JP] Crister::ApplyTransformConversion の fullTransform と厳密に一致
		///      させる。適用 が焼き込む結果をそのままプレビューできるように。
		Matrix baseTransformLinearBasis = Matrix::CreateScale(baseTransformScale_.x, baseTransformScale_.y, baseTransformScale_.z) * Matrix::CreateFromYawPitchRoll(ToRadians(baseTransformRotation_.y), ToRadians(baseTransformRotation_.x), ToRadians(baseTransformRotation_.z));
		context_.modelTransformPreviewContext_.previewWorldMatrix_ = Matrix::CreateTranslation(-baseTransformPivot_) * baseTransformLinearBasis * Matrix::CreateTranslation(baseTransformPivot_ + baseTransformPosition_);

		ImVec2 previewSize = ImGui::GetContentRegionAvail();
		previewSize.y = Max(previewSize.y - ImGui::GetFrameHeightWithSpacing(), 100.0f);
		if (context_.cameraContext_.modelTransformCamera_)
		{
			context_.cameraContext_.modelTransformCamera_->Resize(previewSize.x, previewSize.y);
		}

		ImVec2 imagePosition = ImGui::GetCursorScreenPos();
		ImGui::Image(ImTextureID(previewHandle_.ptr), previewSize);

		/// [EN] Orbit-style preview navigation: left-click drag orbits around
		///      Focus(), middle-click drag pans, wheel dollies - see
		///      PreviewCameraController. Gated on hovering the image (not the
		///      whole child window, so the operation radio buttons below don't
		///      trigger it) and disabled while a gizmo handle is hovered/being
		///      dragged so the two don't fight over the left mouse button.
		/// [JP] オービット式のプレビュー操作: 左クリックドラッグでFocus()を
		///      中心に回転、中クリックドラッグでパン、ホイールでドリー -
		///      PreviewCameraController参照。画像自体のホバーでゲートする
		///      (子ウィンドウ全体ではないので、下の操作モードラジオボタンでは
		///      発火しない)。ギズモのハンドルにホバー/ドラッグ中は左ボタンを
		///      取り合わないよう無効化する。
		Bool orbitHeld = InputSystem::MouseState(InputSystem::MouseButton::Left, InputSystem::IsPressed);
		Bool panHeld = InputSystem::MouseState(InputSystem::MouseButton::Middle, InputSystem::IsPressed);

		if (!ImGuizmo::IsUsing() && !ImGuizmo::IsOver() && ImGui::IsItemHovered() && context_.cameraContext_.modelTransformCamera_ && context_.cameraContext_.modelTransformCameraController_)
		{
			if ((orbitHeld || panHeld) && !InputSystem::IsMouseCaptured())
			{
				InputSystem::BeginMouseCapture();
			}

			context_.cameraContext_.modelTransformCameraController_->Update(*context_.cameraContext_.modelTransformCamera_, ImGui::GetIO().DeltaTime);
		}

		if (!orbitHeld && !panHeld && InputSystem::IsMouseCaptured())
		{
			InputSystem::EndMouseCapture();
		}

		if (ImGui::IsKeyPressed(ImGuiKey_W))
		{
			baseTransformGizmoOperation_ = ImGuizmo::TRANSLATE;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_E))
		{
			baseTransformGizmoOperation_ = ImGuizmo::ROTATE;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_R))
		{
			baseTransformGizmoOperation_ = ImGuizmo::SCALE;
		}

		if (ImGui::RadioButton("移動(W)", baseTransformGizmoOperation_ == ImGuizmo::TRANSLATE))
		{
			baseTransformGizmoOperation_ = ImGuizmo::TRANSLATE;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("回転(E)", baseTransformGizmoOperation_ == ImGuizmo::ROTATE))
		{
			baseTransformGizmoOperation_ = ImGuizmo::ROTATE;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("拡縮(R)", baseTransformGizmoOperation_ == ImGuizmo::SCALE))
		{
			baseTransformGizmoOperation_ = ImGuizmo::SCALE;
		}

		if (context_.cameraContext_.modelTransformCamera_)
		{
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(imagePosition.x, imagePosition.y, previewSize.x, previewSize.y);
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::AllowAxisFlip(false);

			Matrix view = context_.cameraContext_.modelTransformCamera_->View();
			Matrix projection = context_.cameraContext_.modelTransformCamera_->Projection();

			/// [EN] The anchor: pivot/position/rotation/scale collapsed into the
			///      single matrix ImGuizmo manipulates, placed at pivot+position
			///      in world space (a point at local-space pivot lands exactly
			///      there under fullTransform - see ApplyTransformConversion).
			///      Dragging it therefore maps directly onto Position/Rotation/
			///      Scale; Pivot itself has no handle here.
			/// [JP] アンカー: pivot/position/rotation/scale を 1 つの行列へ畳み込み、
			///      ImGuizmo に操作させる。ワールド空間の pivot+position に置く
			///      (ローカル空間の pivot にある点は fullTransform の下でちょうど
			///      そこへ着地する — ApplyTransformConversion 参照)。ドラッグは
			///      そのまま Position/Rotation/Scale に対応する。Pivot 自体の
			///      ハンドルはここには無い。
			Matrix anchorWorld = Matrix::CreateScale(baseTransformScale_.x, baseTransformScale_.y, baseTransformScale_.z) * Matrix::CreateFromYawPitchRoll(ToRadians(baseTransformRotation_.y), ToRadians(baseTransformRotation_.x), ToRadians(baseTransformRotation_.z)) * Matrix::CreateTranslation(baseTransformPivot_ + baseTransformPosition_);

			if (ImGuizmo::Manipulate(&view._11, &projection._11, baseTransformGizmoOperation_, ImGuizmo::LOCAL, &anchorWorld._11))
			{
				Vector3 newScale, newTranslation;
				Quaternion newRotation;
				if (anchorWorld.Decompose(newScale, newRotation, newTranslation))
				{
					baseTransformScale_ = newScale;

					Vector3 euler = newRotation.ToEuler();
					baseTransformRotation_ = Vector3(ToDegrees(euler.x), ToDegrees(euler.y), ToDegrees(euler.z));

					baseTransformPosition_ = newTranslation - baseTransformPivot_;
				}
			}
		}
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
		Asset* asset = context_.worldContext_.resource_->GetAsset(assetId);
		if (!asset)
		{
			return;
		}

		std::filesystem::path path(asset->fullpath_.c_str());
		Bool isSourceAsset = (path.extension() == ".gltf" || path.extension() == ".glb");

		ModelResource* modelResource = context_.worldContext_.resource_->GetModelResource();
		BindlessHeap* heap = context_.worldContext_.resource_->Heap();

		D3D12Context* d3d12Context = context_.graphicsContext_.graphics_->GetContext();
		BC7CompressShader& bc7Shader = context_.graphicsContext_.graphics_->GetBC7CompressShader();

		if (isSourceAsset)
		{
			context_.worldContext_.resource_->WriteAssetMeta(assetId, editConvention_);
			modelResource->Unload(*context_.worldContext_.loader_, assetId, heap);
			modelResource->Load(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), heap, bc7Shader, *context_.worldContext_.resource_, assetId);
		}
		else
		{
			AxisConvention previousConvention = context_.worldContext_.resource_->ReadAxisConvention(assetId);
			ResolvedAxisConvention oldResolved = ResolvedAxisConvention::Resolve(previousConvention);
			ResolvedAxisConvention newResolved = ResolvedAxisConvention::Resolve(editConvention_);
			Matrix deltaBasis = oldResolved.basis_.Transpose() * newResolved.basis_;

			Handle<Crister> handle = modelResource->GetHandle(assetId);
			Crister* crister = handle.empty() ? nullptr : modelResource->Resolve(*context_.worldContext_.loader_, handle);
			if (!crister || !crister->ApplyAxisConversion(deltaBasis, newResolved.flipWinding_, path))
			{
				return;
			}

			context_.worldContext_.resource_->WriteAssetMeta(assetId, editConvention_);
			modelResource->Unload(*context_.worldContext_.loader_, assetId, heap);
			modelResource->Load(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), heap, bc7Shader, *context_.worldContext_.resource_, assetId);
		}
	}

	void ModelTransformPanel::DrawBaseTransformInspector(Uint32 assetId)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextDisabled("基礎トランスフォーム");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Position");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::DragFloat3("##basePosition", &baseTransformPosition_.x, 0.01f);

		ImGui::Text("Rotation");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::DragFloat3("##baseRotation", &baseTransformRotation_.x, 0.1f);

		ImGui::Text("Scale");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::DragFloat3("##baseScale", &baseTransformScale_.x, 0.01f);

		ImGui::Text("Pivot");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::DragFloat3("##basePivot", &baseTransformPivot_.x, 0.01f);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		Asset* asset = context_.worldContext_.resource_->GetAsset(assetId);
		std::filesystem::path path = asset ? std::filesystem::path(asset->fullpath_.c_str()) : std::filesystem::path();
		Bool isSourceAsset = (path.extension() == ".gltf" || path.extension() == ".glb");

		if (isSourceAsset)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "軸コンベンションを一度適用して .crister へ変換してから使用してください");
		}

		ImGui::BeginDisabled(isSourceAsset);
		if (ImGui::Button("適用##baseTransform"))
		{
			ApplyTransformConversion(assetId);
		}
		ImGui::EndDisabled();
	}

	void ModelTransformPanel::ApplyTransformConversion(Uint32 assetId)
	{
		Asset* asset = context_.worldContext_.resource_->GetAsset(assetId);
		if (!asset)
		{
			return;
		}

		std::filesystem::path path(asset->fullpath_.c_str());
		if (path.extension() == ".gltf" || path.extension() == ".glb")
		{
			return;
		}

		ModelResource* modelResource = context_.worldContext_.resource_->GetModelResource();
		Handle<Crister> handle = modelResource->GetHandle(assetId);
		Crister* crister = handle.empty() ? nullptr : modelResource->Resolve(*context_.worldContext_.loader_, handle);
		if (!crister || !crister->ApplyTransformConversion(baseTransformPosition_, baseTransformRotation_, baseTransformScale_, baseTransformPivot_, path))
		{
			return;
		}

		baseTransformPosition_ = Vector3(0.0f, 0.0f, 0.0f);
		baseTransformRotation_ = Vector3(0.0f, 0.0f, 0.0f);
		baseTransformScale_ = Vector3(1.0f, 1.0f, 1.0f);
		baseTransformPivot_ = Vector3(0.0f, 0.0f, 0.0f);

		BindlessHeap* heap = context_.worldContext_.resource_->Heap();
		D3D12Context* d3d12Context = context_.graphicsContext_.graphics_->GetContext();
		modelResource->Unload(*context_.worldContext_.loader_, assetId, heap);
		modelResource->Load(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), heap, context_.graphicsContext_.graphics_->GetBC7CompressShader(), *context_.worldContext_.resource_, assetId);
	}
}

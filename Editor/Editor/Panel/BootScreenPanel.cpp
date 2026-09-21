#include <Editor/Editor/Panel/BootScreenPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <External/ImGui/Include/imgui_internal.h>
#include <GraphicsEngine/Graphics.h>
#include <GraphicsEngine/D3D12/Context/D3D12Context.h>
#include <GraphicsEngine/Shape/Screen/BootScreen.h>
#include <FoundationEngine/Resource/GameConfig.h>
#include <FoundationEngine/File/FileDialog.h>

namespace SeedCore
{
	BootScreenPanel::BootScreenPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void BootScreenPanel::Open()
	{
		show_ = true;

		config_ = BootConfig();
		config_.Load();
		imagesDirty_ = true;

		GameConfig gameConfig;
		gameConfig.Load();
		screenWidth_ = Max<Uint32>(gameConfig.windowWidth_, 1);
		screenHeight_ = Max<Uint32>(gameConfig.windowHeight_, 1);

		ImGui::SetWindowFocus("起動ローディング画面");
	}

	Bool BootScreenPanel::Focused()const
	{
		return isFocused_;
	}

	void BootScreenPanel::Draw()
	{
		context_.bootScreenPreviewContext_.previewActive_ = false;
		context_.bootScreenPreviewContext_.renderer_ = nullptr;
		context_.bootScreenPreviewContext_.config_ = nullptr;
		isFocused_ = false;

		if (!show_)
		{
			return;
		}

		ImGui::DockBuilderDockWindow("起動ローディング画面", context_.graphicsContext_.imgui_->GetDockSpaceID());
		ImGui::SetNextWindowSize(ImVec2(1180, 720), ImGuiCond_FirstUseEver);

		isFocused_ = ImGui::Begin("起動ローディング画面", &show_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		if (isFocused_)
		{
			Graphics* graphics = context_.graphicsContext_.graphics_;

			if (!renderer_)
			{
				renderer_ = MakePtr<BootScreenRenderer>();
				renderer_->Create(graphics->GetContext()->GetDevice(), graphics->GetContext()->GetDirectQueue(), graphics->GetBindlessHeap(), context_.graphicsContext_.imgui_->GetDescriptorHeap(), screenWidth_, screenHeight_);
			}
			else if (renderer_->Width() != screenWidth_ || renderer_->Height() != screenHeight_)
			{
				graphics->WaitForGpuIdle();
				renderer_->Resize(screenWidth_, screenHeight_);
			}

			if (imagesDirty_)
			{
				imagesDirty_ = false;
				renderer_->LoadImages(config_);
			}

			DrawCanvas();

			if (saveRequested_ && !ImGui::IsAnyItemActive() && drag_ == GizmoDrag::None)
			{
				saveRequested_ = false;
				config_.Save();
			}

			if (autoPlay_)
			{
				progress_ += ImGui::GetIO().DeltaTime * 0.2f;
				if (progress_ > 1.25f)
				{
					progress_ = 0.0f;
				}
			}

			context_.bootScreenPreviewContext_.previewActive_ = true;
			context_.bootScreenPreviewContext_.renderer_ = &*renderer_;
			context_.bootScreenPreviewContext_.config_ = &config_;
			context_.bootScreenPreviewContext_.progress_ = Min(progress_, 1.0f);
		}
		ImGui::End();
	}

	void BootScreenPanel::DrawDetails()
	{
		Bool changed = false;

		const Wchar* imageFilterName = L"Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.dds)";
		const Wchar* imageFilterExtension = L"*.png;*.jpg;*.jpeg;*.bmp;*.dds";

		ImGui::SeparatorText("背景");

		if (config_.backgroundImage_.empty())
		{
			ImGui::TextDisabled("デフォルト (Runtime/Logo/ProgressBackground.sub.logo)");
		}
		else
		{
			ImGui::TextDisabled("カスタム (%zu KB)", config_.backgroundImage_.size() / 1024);
		}
		if (ImGui::Button("画像を選択...##Background"))
		{
			std::filesystem::path pickedImagePath;
			if (FileDialog::OpenFile(pickedImagePath, std::filesystem::current_path(), imageFilterName, imageFilterExtension) && BootConfig::Import(pickedImagePath, config_.backgroundImage_))
			{
				imagesDirty_ = true;
				changed = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("デフォルトに戻す##Background") && !config_.backgroundImage_.empty())
		{
			config_.backgroundImage_.clear();
			imagesDirty_ = true;
			changed = true;
		}
		changed |= ImGui::Checkbox("画像を使う##Background", &config_.useBackgroundImage_);
		ImGui::BeginDisabled(!config_.useBackgroundImage_);
		changed |= ImGui::ColorEdit4("乗算色##Background", &config_.backgroundTint_.x);
		ImGui::EndDisabled();
		changed |= ImGui::ColorEdit3("背景色##Background", &config_.backgroundColor_.x);

		ImGui::SeparatorText("バー");

		if (config_.barImage_.empty())
		{
			ImGui::TextDisabled("デフォルト (Runtime/Logo/ProgressBar.sub.logo)");
		}
		else
		{
			ImGui::TextDisabled("カスタム (%zu KB)", config_.barImage_.size() / 1024);
		}
		if (ImGui::Button("画像を選択...##Bar"))
		{
			std::filesystem::path pickedImagePath;
			if (FileDialog::OpenFile(pickedImagePath, std::filesystem::current_path(), imageFilterName, imageFilterExtension) && BootConfig::Import(pickedImagePath, config_.barImage_))
			{
				imagesDirty_ = true;
				changed = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("デフォルトに戻す##Bar") && !config_.barImage_.empty())
		{
			config_.barImage_.clear();
			imagesDirty_ = true;
			changed = true;
		}
		changed |= ImGui::ColorEdit4("乗算色##Bar", &config_.barTint_.x);

		ImGui::SeparatorText("枠");

		if (config_.frameImage_.empty())
		{
			ImGui::TextDisabled("デフォルト (Runtime/Logo/ProgressFrame.sub.logo)");
		}
		else
		{
			ImGui::TextDisabled("カスタム (%zu KB)", config_.frameImage_.size() / 1024);
		}
		if (ImGui::Button("画像を選択...##Frame"))
		{
			std::filesystem::path pickedImagePath;
			if (FileDialog::OpenFile(pickedImagePath, std::filesystem::current_path(), imageFilterName, imageFilterExtension) && BootConfig::Import(pickedImagePath, config_.frameImage_))
			{
				imagesDirty_ = true;
				changed = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("デフォルトに戻す##Frame") && !config_.frameImage_.empty())
		{
			config_.frameImage_.clear();
			imagesDirty_ = true;
			changed = true;
		}
		changed |= ImGui::Checkbox("表示する##Frame", &config_.useFrame_);
		ImGui::BeginDisabled(!config_.useFrame_);
		changed |= ImGui::ColorEdit4("乗算色##Frame", &config_.frameTint_.x);
		ImGui::EndDisabled();
		ImGui::TextWrapped("枠はバーの矩形に合わせて伸縮します。バー画像と同じ縦横比で作ってください。");

		ImGui::SeparatorText("配置");

		const Char* anchorLabels[] = { "左上", "上", "右上", "左", "中央", "右", "左下", "下", "右下" };
		Int32 anchorIndex = static_cast<Int32>(config_.anchor_);
		if (ImGui::Combo("基準", &anchorIndex, anchorLabels, IM_ARRAYSIZE(anchorLabels)))
		{
			config_.anchor_ = static_cast<BootAnchor>(anchorIndex);

			Float marginY = 0.05f;
			Float marginX = marginY * static_cast<Float>(screenHeight_) / static_cast<Float>(screenWidth_);
			Int32 column = anchorIndex % 3;
			Int32 row = anchorIndex / 3;
			config_.offset_.x = column == 0 ? marginX : (column == 2 ? -marginX : 0.0f);
			config_.offset_.y = row == 0 ? marginY : (row == 2 ? -marginY : 0.0f);
			changed = true;
		}
		changed |= ImGui::DragFloat2("オフセット", &config_.offset_.x, 0.001f, -1.0f, 1.0f, "%.3f");
		changed |= ImGui::DragFloat("幅", &config_.width_, 0.001f, 0.01f, 1.0f, "%.3f");
		ImGui::TextDisabled("オフセットと幅は画面サイズに対する割合です。");

		ImGui::SeparatorText("動き");

		const Char* fillMethodLabels[] = { "横", "縦", "扇形 (90°)", "扇形 (180°)", "円形 (360°)" };
		Int32 fillMethodIndex = static_cast<Int32>(config_.fillMethod_);
		if (ImGui::Combo("塗り方", &fillMethodIndex, fillMethodLabels, IM_ARRAYSIZE(fillMethodLabels)))
		{
			config_.fillMethod_ = static_cast<BootFillMethod>(fillMethodIndex);
			config_.fillOrigin_ = 0;
			changed = true;
		}

		const Char* horizontalOriginLabels[] = { "左", "右" };
		const Char* verticalOriginLabels[] = { "下", "上" };
		const Char* radial90OriginLabels[] = { "左下", "左上", "右上", "右下" };
		const Char* radial180OriginLabels[] = { "下", "左", "上", "右" };
		const Char* radial360OriginLabels[] = { "下", "右", "上", "左" };

		const Char* const* originLabels = horizontalOriginLabels;
		Int32 originCount = 2;
		if (config_.fillMethod_ == BootFillMethod::Vertical)
		{
			originLabels = verticalOriginLabels;
		}
		else if (config_.fillMethod_ == BootFillMethod::Radial90)
		{
			originLabels = radial90OriginLabels;
			originCount = 4;
		}
		else if (config_.fillMethod_ == BootFillMethod::Radial180)
		{
			originLabels = radial180OriginLabels;
			originCount = 4;
		}
		else if (config_.fillMethod_ == BootFillMethod::Radial360)
		{
			originLabels = radial360OriginLabels;
			originCount = 4;
		}

		config_.fillOrigin_ = std::clamp(config_.fillOrigin_, 0, originCount - 1);
		changed |= ImGui::Combo("開始位置", &config_.fillOrigin_, originLabels, originCount);

		Bool isRadial = config_.fillMethod_ == BootFillMethod::Radial90 || config_.fillMethod_ == BootFillMethod::Radial180 || config_.fillMethod_ == BootFillMethod::Radial360;
		if (isRadial)
		{
			changed |= ImGui::Checkbox("時計回り", &config_.clockwise_);
		}
		else
		{
			changed |= ImGui::Checkbox("先端を波打たせる", &config_.wave_);
		}

		ImGui::SeparatorText("プレビュー");

		ImGui::Checkbox("自動再生", &autoPlay_);
		Float previewProgress = Min(progress_, 1.0f);
		if (ImGui::SliderFloat("進捗", &previewProgress, 0.0f, 1.0f, "%.2f"))
		{
			progress_ = previewProgress;
			autoPlay_ = false;
		}
		if (ImGui::Button("表示を合わせる"))
		{
			zoom_ = 1.0f;
			pan_ = Vector2(0.0f, 0.0f);
		}
		ImGui::TextDisabled("画面サイズ: %u x %u (ゲーム設定のウィンドウサイズ)", screenWidth_, screenHeight_);
		ImGui::TextDisabled("ホイール: 拡大縮小 / 中・右ドラッグ: 移動");

		if (changed)
		{
			saveRequested_ = true;
		}
	}

	void BootScreenPanel::DrawCanvas()
	{
		ImGuiIO& io = ImGui::GetIO();

		ImVec2 canvasPosition = ImGui::GetCursorScreenPos();
		ImVec2 canvasSize = ImGui::GetContentRegionAvail();
		canvasSize.x = Max(canvasSize.x, 64.0f);
		canvasSize.y = Max(canvasSize.y, 64.0f);

		ImGui::InvisibleButton("##BootScreenCanvasArea", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
		Bool canvasHovered = ImGui::IsItemHovered();
		Bool canvasActive = ImGui::IsItemActive();

		Float screenWidth = static_cast<Float>(screenWidth_);
		Float screenHeight = static_cast<Float>(screenHeight_);

		Vector2 canvasCenter = Vector2(canvasPosition.x + canvasSize.x * 0.5f, canvasPosition.y + canvasSize.y * 0.5f);
		Vector2 mouse = Vector2(io.MousePos.x, io.MousePos.y);

		if (canvasHovered && io.MouseWheel != 0.0f)
		{
			Float previousZoom = zoom_;
			zoom_ = std::clamp(zoom_ * std::pow(1.1f, io.MouseWheel), 0.1f, 10.0f);

			Vector2 mouseFromCenter = mouse - (canvasCenter + pan_);
			pan_ += mouseFromCenter - mouseFromCenter * (zoom_ / previousZoom);
		}

		if (canvasActive && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || ImGui::IsMouseDragging(ImGuiMouseButton_Right)))
		{
			pan_ += Vector2(io.MouseDelta.x, io.MouseDelta.y);
		}

		Float fitScale = Min(canvasSize.x * 0.9f / screenWidth, canvasSize.y * 0.9f / screenHeight);
		Float scale = fitScale * zoom_;
		Vector2 imageSize = Vector2(screenWidth * scale, screenHeight * scale);
		Vector2 imageMin = canvasCenter + pan_ - imageSize * 0.5f;
		Vector2 imageMax = imageMin + imageSize;

		Vector4 barRect = BootScreen::BarRect(config_, screenWidth, screenHeight, renderer_->BarAspect());
		Vector2 barMin = imageMin + Vector2(barRect.x, barRect.y) * imageSize;
		Vector2 barMax = imageMin + Vector2(barRect.z, barRect.w) * imageSize;

		Vector2 corners[4] = { barMin, Vector2(barMax.x, barMin.y), Vector2(barMin.x, barMax.y), barMax };
		GizmoDrag cornerDrags[4] = { GizmoDrag::TopLeft, GizmoDrag::TopRight, GizmoDrag::BottomLeft, GizmoDrag::BottomRight };
		Float handleRadius = 6.0f;

		GizmoDrag hoveredDrag = GizmoDrag::None;
		if (canvasHovered)
		{
			for (Int32 cornerIndex = 0; cornerIndex < 4; cornerIndex++)
			{
				if ((mouse - corners[cornerIndex]).Length() <= handleRadius + 3.0f)
				{
					hoveredDrag = cornerDrags[cornerIndex];
					break;
				}
			}

			if (hoveredDrag == GizmoDrag::None && mouse.x >= barMin.x && mouse.x <= barMax.x && mouse.y >= barMin.y && mouse.y <= barMax.y)
			{
				hoveredDrag = GizmoDrag::Move;
			}
		}

		if (drag_ == GizmoDrag::None && hoveredDrag != GizmoDrag::None && ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			drag_ = hoveredDrag;
			dragStartMouse_ = mouse;
			dragStartOffset_ = config_.offset_;
			dragStartRect_ = barRect;
		}

		if (drag_ != GizmoDrag::None)
		{
			if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				drag_ = GizmoDrag::None;
				saveRequested_ = true;
			}
			else
			{
				Vector2 deltaUv = (mouse - dragStartMouse_) / imageSize;

				if (drag_ == GizmoDrag::Move)
				{
					config_.offset_ = dragStartOffset_ + deltaUv;
				}
				else
				{
					Bool draggingRight = drag_ == GizmoDrag::TopRight || drag_ == GizmoDrag::BottomRight;
					Bool draggingBottom = drag_ == GizmoDrag::BottomLeft || drag_ == GizmoDrag::BottomRight;

					Vector2 fixedCorner = Vector2(draggingRight ? dragStartRect_.x : dragStartRect_.z, draggingBottom ? dragStartRect_.y : dragStartRect_.w);
					Float draggedCornerX = (draggingRight ? dragStartRect_.z : dragStartRect_.x) + deltaUv.x;

					Float width = std::clamp(std::abs(draggedCornerX - fixedCorner.x), 0.01f, 2.0f);
					Float height = width * screenWidth / (Max(renderer_->BarAspect(), 0.0001f) * screenHeight);

					Vector2 newMin = Vector2(draggingRight ? fixedCorner.x : fixedCorner.x - width, draggingBottom ? fixedCorner.y : fixedCorner.y - height);

					Int32 anchorIndex = static_cast<Int32>(config_.anchor_);
					Vector2 anchor = Vector2(static_cast<Float>(anchorIndex % 3) * 0.5f, static_cast<Float>(anchorIndex / 3) * 0.5f);

					config_.width_ = width;
					config_.offset_ = newMin - anchor + anchor * Vector2(width, height);
				}
			}
		}

		GizmoDrag cursorDrag = drag_ != GizmoDrag::None ? drag_ : hoveredDrag;
		if (cursorDrag == GizmoDrag::Move)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
		}
		else if (cursorDrag == GizmoDrag::TopLeft || cursorDrag == GizmoDrag::BottomRight)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
		}
		else if (cursorDrag == GizmoDrag::TopRight || cursorDrag == GizmoDrag::BottomLeft)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
		}

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->PushClipRect(canvasPosition, ImVec2(canvasPosition.x + canvasSize.x, canvasPosition.y + canvasSize.y), true);

		drawList->AddRectFilled(canvasPosition, ImVec2(canvasPosition.x + canvasSize.x, canvasPosition.y + canvasSize.y), IM_COL32(32, 32, 32, 255));
		drawList->AddImage(ImTextureID(renderer_->ImGuiGPUHandle().ptr), ImVec2(imageMin.x, imageMin.y), ImVec2(imageMax.x, imageMax.y));
		drawList->AddRect(ImVec2(imageMin.x, imageMin.y), ImVec2(imageMax.x, imageMax.y), IM_COL32(110, 110, 110, 255));

		Int32 anchorIndex = static_cast<Int32>(config_.anchor_);
		Vector2 anchorPoint = imageMin + Vector2(static_cast<Float>(anchorIndex % 3) * 0.5f, static_cast<Float>(anchorIndex / 3) * 0.5f) * imageSize;
		drawList->AddLine(ImVec2(anchorPoint.x - 8.0f, anchorPoint.y), ImVec2(anchorPoint.x + 8.0f, anchorPoint.y), IM_COL32(80, 200, 255, 255), 2.0f);
		drawList->AddLine(ImVec2(anchorPoint.x, anchorPoint.y - 8.0f), ImVec2(anchorPoint.x, anchorPoint.y + 8.0f), IM_COL32(80, 200, 255, 255), 2.0f);

		ImU32 gizmoColor = drag_ != GizmoDrag::None || hoveredDrag != GizmoDrag::None ? IM_COL32(255, 200, 60, 255) : IM_COL32(255, 160, 40, 220);
		drawList->AddRect(ImVec2(barMin.x, barMin.y), ImVec2(barMax.x, barMax.y), gizmoColor, 0.0f, 0, 1.5f);

		for (Int32 cornerIndex = 0; cornerIndex < 4; cornerIndex++)
		{
			Bool highlighted = cursorDrag == cornerDrags[cornerIndex];
			drawList->AddCircleFilled(ImVec2(corners[cornerIndex].x, corners[cornerIndex].y), handleRadius, highlighted ? IM_COL32(255, 200, 60, 255) : IM_COL32(255, 255, 255, 255));
			drawList->AddCircle(ImVec2(corners[cornerIndex].x, corners[cornerIndex].y), handleRadius, IM_COL32(40, 40, 40, 255), 0, 1.5f);
		}

		drawList->PopClipRect();
	}
}

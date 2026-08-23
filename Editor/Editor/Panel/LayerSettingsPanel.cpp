#include <Editor/Editor/Panel/LayerSettingsPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <FoundationEngine/ECS/LayerRegistry.h>
#include <FoundationEngine/ECS/LayerCollisionMatrix.h>

namespace SeedCore
{

	LayerSettingsPanel::LayerSettingsPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void LayerSettingsPanel::Open()
	{
		show_ = true;
	}

	void LayerSettingsPanel::DrawCollisionMatrixTab()
	{
		ImGui::TextDisabled("2つのレイヤーが物理的に衝突してよいかを設定します");
		ImGui::Spacing();

		DynamicArray<Size> usedLayers;
		for (Size index = 0; index < LayerRegistry::LayerCount; ++index)
		{
			if (LayerRegistry::IsUsed(index))
			{
				usedLayers.push_back(index);
			}
		}

		if (usedLayers.empty())
		{
			ImGui::TextDisabled("(名前付きレイヤーがありません)");
			return;
		}

		const DynamicArray<String>& names = LayerRegistry::GetNames();

		/// [EN] Column header rotation angle - see the header-row drawing
		///      code inside BeginTable() below for the full rationale.
		/// [JP] 列見出しの回転角度 - 詳細な理由は下の BeginTable() 内の
		///      見出し行描画コードを参照。
		constexpr Float headerRotation = 1.5707963f; // 90度（画面座標のY下方向で、水平テキストが下向きに倒れる回転）

		Float maxNameWidth = 0.0f;
		for (Size layerIndex : usedLayers)
		{
			maxNameWidth = Max(maxNameWidth, ImGui::CalcTextSize(names[layerIndex].c_str()).x);
		}
		Float headerRowHeight = maxNameWidth + 40.0f;
		constexpr Float rowLabelWidth = 110.0f;

		/// [EN] Column width has to fit two different things: the
		///      checkbox cells below, and the rotated header text above -
		///      where the text's line height becomes its HORIZONTAL
		///      footprint once rotated 90 degrees. Sizing purely off the
		///      checkbox (GetFrameHeight()) left barely any gap between
		///      neighboring rotated labels, so they read as one run-on
		///      string instead of separate names.
		/// [JP] 列幅は2つのものに合わせる必要がある: 下のチェックボックス
		///      セルと、上の回転済み見出しテキスト - テキストは90度回転
		///      すると、その行の高さがそのまま横方向の幅になる。
		///      チェックボックス(GetFrameHeight())だけを基準にすると、
		///      隣り合う回転見出し同士の間にほとんど余白が無く、それぞれの
		///      別々の名前ではなく1本の続き文字のように見えてしまっていた。
		Float checkboxColumnWidth = ImGui::GetFrameHeight() + 6.0f;
		Float headerTextColumnWidth = ImGui::GetTextLineHeight() + 18.0f;
		Float columnWidth = Max(checkboxColumnWidth, headerTextColumnWidth);

		/// [EN] The header row is drawn as the table's own first row -
		///      not in separate outside-the-table layout space - so its
		///      borders and column boundaries are the table's real ones,
		///      not a hand-drawn approximation that can drift out of
		///      sync. AddText() alone would still clip a rotated column
		///      header's glyphs against the cell's narrow, un-rotated
		///      width (a documented ImGui limitation: it clips text
		///      against its pre-rotation footprint - see
		///      github.com/ocornut/imgui/issues/1286); widening the clip
		///      rect around each AddText() call, below, is the documented
		///      fix - widened only to that string's own un-rotated
		///      footprint (not the whole window), and further clamped
		///      into this window's own clip rect (captured further
		///      down), so a header neither bleeds into neighboring
		///      columns nor draws outside the popup window.
		/// [JP] 見出し行はテーブル外の別レイアウト空間にではなく、テーブル
		///      自身の最初の行として描画する - そうすることで枠線や列境界が
		///      テーブル本物のものになり、ずれうる手描き近似にならない。
		///      AddText() だけでは、回転済み列見出しのグリフがセルの
		///      (回転前の)狭い幅でクリップされてしまう(既知のImGuiの制限:
		///      回転前の見た目でクリップしてしまう -
		///      github.com/ocornut/imgui/issues/1286 参照) - 下の各
		///      AddText() 呼び出しの間だけクリップ矩形を広げるのが公式な
		///      対処法。広げる範囲はその文字列自身の(回転前の)footprint
		///      だけに留め(ウィンドウ全体ではない)、さらに(さらに下で
		///      取得する)このウィンドウ自身のクリップ矩形の内側にクランプ
		///      することで、見出しが隣の列へはみ出すことも、ポップアップ
		///      ウィンドウの外へ描画されることも無くなる。

		/// [EN] All LayerRegistry::LayerCount slots are always named now
		///      (unrenamed slots default to "Layer N"), so this table
		///      always renders the full grid, which can be wider/taller
		///      than the window. ScrollX/ScrollY make the table create
		///      its own internal scrolling region sized to outer_size
		///      below (an outer BeginChild() wrapping a non-ScrollX table
		///      does NOT work - without ScrollX on the table itself, its
		///      default sizing policy is SizingStretchSame and its width
		///      is clamped to fit the available space, so fixed columns
		///      get silently squeezed instead of ever overflowing into a
		///      scrollbar).
		/// [JP] LayerRegistry::LayerCount 個のスロットは常に名前を持つ
		///      ようになったため(リネームされていないスロットは既定で
		///      "Layer N")、このテーブルは常にフルグリッドを描画する -
		///      ウィンドウより縦横ともに大きくなり得る。ScrollX/ScrollY を
		///      付けることで、テーブル自身が下の outer_size のサイズで
		///      内部スクロール領域を作る(ScrollXの無いテーブルを外側の
		///      BeginChild() で包むだけでは効かない - テーブル自身に
		///      ScrollX が無いと既定のサイズ決定方針は SizingStretchSame に
		///      なり、幅が利用可能なスペースに収まるようクランプされるため、
		///      固定列はスクロールバーへあふれる前に黙って縮められてしまう)。
		/// [EN] Fills the rest of the window's height, reserving just
		///      enough room below for the "すべて無効"/"すべて有効" buttons
		///      - previously capped at a fixed 420px, which left a large
		///      dead area under the buttons whenever the window was
		///      opened taller than that.
		/// [JP] ウィンドウの残り高さいっぱいまで広げる。下に「すべて無効」
		///      「すべて有効」ボタン分の余白だけ確保する - 以前は固定420pxで
		///      頭打ちにしていたため、ウィンドウをそれより高く開くと
		///      ボタンの下に大きな空白ができていた。
		Float tableHeight = Max(ImGui::GetContentRegionAvail().y - 60.0f, 200.0f);

		/// [EN] Bounds for the header text's widened clip rect (see the
		///      PushClipRect() call further down). Taken from the
		///      window's own position/size rather than the draw list's
		///      current clip rect stack - the stack top can already be
		///      narrower than the window itself depending on what was
		///      drawn just before this point, and a clamp range with
		///      min.x > max.x collapses every header to a zero-width
		///      (invisible) clip rect.
		/// [JP] 見出しテキストの拡大クリップ矩形の範囲(さらに下の
		///      PushClipRect() 呼び出しを参照)。描画リストの現在の
		///      クリップ矩形スタックではなく、ウィンドウ自身の位置/
		///      サイズから取る - スタックの先頭は、この時点の直前に
		///      何を描画したかによってはウィンドウ自体より狭くなって
		///      いることがあり、min.x > max.x なクランプ範囲は全ての
		///      見出しを幅ゼロ(不可視)のクリップ矩形に潰してしまう。
		ImVec2 windowClipMin = ImGui::GetWindowPos();
		ImVec2 windowClipMax = ImVec2(windowClipMin.x + ImGui::GetWindowSize().x, windowClipMin.y + ImGui::GetWindowSize().y);

		/// [EN] outer_size.x = 0 would mean "use all remaining window
		///      width", leaving a dead gap to the right of the last
		///      column whenever the window is wider than the actual
		///      column content - shrink the table's own viewport down
		///      to the real content width instead (still clamped to the
		///      available width, so it keeps scrolling when the window
		///      is narrower than the content).
		/// [JP] outer_size.x を 0 にすると「残りのウィンドウ幅を全部使う」
		///      意味になり、ウィンドウが実際の列ぶんのコンテンツより
		///      広い場合、最後の列の右側に無駄な空白が残ってしまう -
		///      代わりにテーブル自身の表示領域を実際のコンテンツ幅まで
		///      縮める(利用可能な幅でクランプするので、ウィンドウが
		///      コンテンツより狭い場合は引き続きスクロールする)。
		/// [EN] The margin mirrors the window's own bottom inset - the gap
		///      that's always there below the last content row (here, the
		///      "すべて無効"/"すべて有効" buttons) down to the window's
		///      edge - applied to the right side instead. Subtracted from
		///      the AVAILABLE width (not added into the table's own
		///      outer_size/content width) so it lands as clean empty
		///      window space outside the table, not as a blank bordered
		///      strip inside the table's own scrolling area.
		/// [JP] 余白は、ウィンドウ自身の下側の余白 - 最後のコンテンツ
		///      (ここでは「すべて無効」「すべて有効」ボタン)の下から
		///      ウィンドウの端までいつも存在している余白 - と同じものを
		///      右側に付ける。テーブル自身の outer_size/コンテンツ幅に
		///      足し込むのではなく、利用可能な幅の方から差し引く - そう
		///      することで、テーブルの枠線付きスクロール領域の内側に
		///      空白の帯ができるのではなく、テーブルの外側のウィンドウの
		///      素の空きスペースとして現れる。
		Float contentWidth = rowLabelWidth + columnWidth * static_cast<Float>(usedLayers.size());
		Float availableWidth = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().WindowPadding.y;
		Float tableWidth = Min(contentWidth, availableWidth);

		Int columnCount = static_cast<Int>(usedLayers.size()) + 1;
		if (ImGui::BeginTable("##LayerCollisionMatrix", columnCount, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY, ImVec2(tableWidth, tableHeight)))
		{
			ImGui::TableSetupColumn("##RowLabel", ImGuiTableColumnFlags_WidthFixed, rowLabelWidth);
			for (Size layerIndex : usedLayers)
			{
				(void)layerIndex;
				ImGui::TableSetupColumn("##Col", ImGuiTableColumnFlags_WidthFixed, columnWidth);
			}

			/// [EN] Column headers are drawn as the ordinary horizontal
			///      string, rotated 90 degrees so it reads top-to-bottom
			///      (like a spreadsheet's rotated column headers) - natural
			///      font shapes/kerning throughout, instead of stacking
			///      individual upright characters. Columns are walked in
			///      the same REVERSED order as the data rows below (see the
			///      note further down), so each header lines up with its
			///      own column.
			/// [JP] 列見出しは普通の横書き文字列を90度回転させ、上から下へ
			///      読めるようにする（表計算ソフトの回転見出しと同じ）-
			///      1文字ずつ立てて積むのではなく、フォント本来の字形/字間を
			///      そのまま使う。列は下のデータ行と同じ逆順で走査し(詳細は
			///      さらに下の注釈を参照)、各見出しが自分の列と揃うようにする。
			ImGui::TableNextRow(ImGuiTableRowFlags_None, headerRowHeight);
			ImGui::TableSetColumnIndex(0);

			Float cosAngle = std::cos(headerRotation);
			Float sinAngle = std::sin(headerRotation);

			for (Size displayCol = 0; displayCol < usedLayers.size(); ++displayCol)
			{
				Size columnPos = usedLayers.size() - 1 - displayCol;

				ImGui::TableSetColumnIndex(static_cast<Int>(displayCol) + 1);

				std::string name = names[usedLayers[columnPos]].str();
				ImVec2 textSize = ImGui::CalcTextSize(name.c_str());

				/// [EN] AddText's pos is the string's un-rotated top-left
				///      corner and stays fixed under rotation; after a 90
				///      degree turn the string's width becomes its
				///      vertical extent (downward from pos) and its height
				///      becomes its horizontal extent (leftward from pos),
				///      so shifting pos right by half the text height
				///      centers the now-vertical string on the cell.
				/// [JP] AddText の pos は文字列の(回転前の)左上角で、回転後も
				///      その点は固定されたまま - 90度回転すると文字列の幅が
				///      (posから下方向への)縦の広がりに、高さが(posから
				///      左方向への)横の広がりになる。そのため pos を文字の
				///      高さの半分だけ右にずらすと、縦になった文字列がセルの
				///      中心に来る。
				ImVec2 cellMin = ImGui::GetCursorScreenPos();
				Float cellWidth = ImGui::GetContentRegionAvail().x;

				ImDrawList* drawList = ImGui::GetWindowDrawList();

				/// [EN] A column straddling the table's own scroll
				///      boundary is only PARTIALLY inside the table's
				///      native (pre-override) clip rect - drawing our
				///      rotated text for it anyway produced a
				///      half-legible, ragged-looking label right at the
				///      scroll edge. Skip it entirely instead; it draws
				///      cleanly once scrolled fully into view.
				/// [JP] テーブル自身のスクロール境界にまたがっている列は、
				///      テーブル本来の(上書き前の)クリップ矩形に一部しか
				///      収まっていない - それでも回転済みテキストを描画
				///      すると、スクロール境界のところで中途半端で
				///      ガタガタした見た目になっていた。代わりに丸ごと
				///      描画をスキップする - 完全にスクロールして見える
				///      ようになれば、きれいに描画される。
				ImVec2 nativeClipMin = drawList->GetClipRectMin();
				ImVec2 nativeClipMax = drawList->GetClipRectMax();
				Bool columnFullyVisible = (nativeClipMax.x - nativeClipMin.x) >= cellWidth - 1.0f;

				if (!columnFullyVisible)
				{
					continue;
				}

				ImVec2 pivot(cellMin.x + cellWidth * 0.5f + textSize.y * 0.5f, cellMin.y + 6.0f);
				ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

				/// [EN] Widen the active clip rect for the AddText() call
				///      itself - see the note above BeginTable() - then
				///      rotate only the vertices AddText() just produced.
				///      Widened to this window's own clip rect (captured
				///      before BeginTable()), not the narrow per-column
				///      one, since the un-rotated text is wider than its
				///      own column for every header here.
				/// [JP] AddText() 呼び出し自体の間だけ有効なクリップ矩形を
				///      広げる - 詳細は BeginTable() の上の注釈を参照 -
				///      その後、AddText() が生成した頂点だけを回転させる。
				///      狭い列単位のクリップではなく、(BeginTable() の前に
				///      取得した)このウィンドウ自身のクリップ矩形まで広げる
				///      - ここでは全ての見出しで、回転前の文字が自分の列幅
				///      より広いため。
				drawList->PushClipRect(windowClipMin, windowClipMax, false);
				Int vertexStart = drawList->VtxBuffer.Size;
				drawList->AddText(pivot, textColor, name.c_str());
				Int vertexEnd = drawList->VtxBuffer.Size;
				drawList->PopClipRect();

				for (Int vertexIndex = vertexStart; vertexIndex < vertexEnd; ++vertexIndex)
				{
					ImDrawVert& vertex = drawList->VtxBuffer[vertexIndex];
					ImVec2 offset(vertex.pos.x - pivot.x, vertex.pos.y - pivot.y);
					vertex.pos = ImVec2(pivot.x + offset.x * cosAngle - offset.y * sinAngle, pivot.y + offset.x * sinAngle + offset.y * cosAngle);
				}
			}

			/// [EN] Rows stay in ascending LayerRegistry order top-to-bottom
			///      (Default at the top, the highest used index at the
			///      bottom). Columns are walked left-to-right in
			///      REVERSED order instead (the highest used index
			///      leftmost, Default rightmost). Only the upper
			///      triangle by original index (columnPos >= rowPos) is
			///      drawn. A natural-order square only ever produces a
			///      right angle at the top-right (col >= row) or
			///      bottom-left (col <= row) corner; reversing which
			///      original column each display position shows is what
			///      moves that right angle to the top-left instead,
			///      without touching the row order.
			/// [JP] 行は上から下へLayerRegistryの昇順のまま(Defaultが
			///      一番上、使用中の最大インデックスが一番下)。列は逆に、
			///      左から右へ逆順に走査する(使用中の最大インデックスが
			///      一番左、Defaultが一番右)。元のインデックスでの上三角
			///      (columnPos >= rowPos)のみを描画する。両軸とも自然な
			///      昇順のままだと、直角は右上(col >= row)か左下
			///      (col <= row)にしかならない。行の順序には触れず、各
			///      表示位置がどの元の列を表すかを逆にすることで、その
			///      直角を左上へ移せる。
			for (Size rowPos = 0; rowPos < usedLayers.size(); ++rowPos)
			{
				Size layerA = usedLayers[rowPos];

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(names[layerA].c_str());

				for (Size displayCol = 0; displayCol < usedLayers.size(); ++displayCol)
				{
					Size columnPos = usedLayers.size() - 1 - displayCol;

					ImGui::TableSetColumnIndex(static_cast<Int>(displayCol) + 1);

					if (columnPos < rowPos)
					{
						continue;
					}

					Size layerB = usedLayers[columnPos];

					/// [EN] Checkbox() left-aligns itself within the cell by
					///      default - center it on the measured column
					///      center so it lines up with the header above.
					/// [JP] Checkbox() は既定でセル内で左揃えになる -
					///      測定済みの列中心に合わせて中央に置き、上の
					///      見出しと揃うようにする。
					Float checkboxOffsetX = (ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight()) * 0.5f;
					if (checkboxOffsetX > 0.0f)
					{
						ImGui::SetCursorPosX(ImGui::GetCursorPosX() + checkboxOffsetX);
					}

					ImGui::PushID(static_cast<Int>(rowPos * LayerRegistry::LayerCount + columnPos));
					Bool collide = LayerCollisionMatrix::GetCollide(layerA, layerB);
					if (ImGui::Checkbox("##Cell", &collide))
					{
						LayerCollisionMatrix::SetCollide(layerA, layerB, collide);
						LayerRegistry::Save();
					}
					ImGui::PopID();
				}
			}

			ImGui::EndTable();
		}

		ImGui::Spacing();

		if (ImGui::Button("すべて無効"))
		{
			for (Size rowPos = 0; rowPos < usedLayers.size(); ++rowPos)
			{
				for (Size columnPos = rowPos; columnPos < usedLayers.size(); ++columnPos)
				{
					LayerCollisionMatrix::SetCollide(usedLayers[rowPos], usedLayers[columnPos], false);
				}
			}
			LayerRegistry::Save();
		}
		ImGui::SameLine();
		if (ImGui::Button("すべて有効"))
		{
			for (Size rowPos = 0; rowPos < usedLayers.size(); ++rowPos)
			{
				for (Size columnPos = rowPos; columnPos < usedLayers.size(); ++columnPos)
				{
					LayerCollisionMatrix::SetCollide(usedLayers[rowPos], usedLayers[columnPos], true);
				}
			}
			LayerRegistry::Save();
		}
	}

	void LayerSettingsPanel::Draw()
	{
		if (!show_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		/// [EN] All LayerRegistry::LayerCount slots are always named now,
		///      so the collision matrix is always the full grid - open
		///      wide enough for all of them to fit without an immediate
		///      manual resize.
		/// [JP] LayerRegistry::LayerCount 個のスロットは常に名前を持つ
		///      ようになったため、衝突マトリクスは常にフルグリッドになる -
		///      手動でリサイズしなくても全部収まるよう、初期から広めに開く。
		ImGui::SetNextWindowSize(ImVec2(860, 800), ImGuiCond_Appearing);

		/// [EN] The collision matrix scrolls within its own fixed-height
		///      child (see DrawCollisionMatrixTab()), so this outer
		///      window only ever needs to scroll vertically for its
		///      remaining content (description text, the child itself,
		///      the buttons below it) - the default ImGui behavior.
		/// [JP] 衝突マトリクスは自身の固定高さの子ウィンドウ内でスクロール
		///      する(DrawCollisionMatrixTab() 参照)ため、この外側の
		///      ウィンドウは残りのコンテンツ(説明文、子ウィンドウ自体、
		///      その下のボタン)に対して縦方向にだけスクロールできれば
		///      よい - ImGui既定の挙動のまま。
		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking;

		if (ImGui::Begin("レイヤー設定", &show_, flags))
		{
			DrawCollisionMatrixTab();
		}
		ImGui::End();
	}
}

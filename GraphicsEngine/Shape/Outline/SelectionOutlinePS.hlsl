#include "../../Shader/Structured.hlsli"
#include "../../Shader/Constants.hlsli"

struct SelectionOutlineOutput
{
	float4 position : SV_Position;
	float2 texcoord : TEXCOORD0;
};

static const float3 OUTLINE_COLOR = float3(0.95, 0.55, 0.05);
static const int OUTLINE_THICKNESS = 2;

/**
* [EN]
* Selection outline composite pixel shader. Against the mask
* DrawSelectionMask wrote (selected mesh = 1, everything else = 0),
* paints pixels selection color only if they are outside the selected
* mesh AND within OUTLINE_THICKNESS pixels of it; every other pixel is
* discarded, leaving the render target's existing content untouched.
* mask is always native-resolution, but this pass may be drawing onto a
* render target at a DIFFERENT resolution (the post-tonemap debug
* overlay drawn onto PostProcessRenderer's DLSS-RR-upscaled output - see
* Renderer::EndEditorFrame), so SV_Position is rescaled by
* screen_size_/display_size_ before indexing mask - a no-op
* (ratio 1.0) whenever the two already match.
*
* ---------------------------------------------------------------------
*
* [JP]
* 選択アウトライン合成用ピクセルシェーダ。DrawSelectionMask が書いたマスク
* （選択メッシュ=1、それ以外=0）に対し、自身が選択メッシュの外側にあり、かつ
* 周囲 OUTLINE_THICKNESS ピクセル以内に選択メッシュがあるピクセルだけを
* 縁取り色で塗る。それ以外は discard してエディタフレームバッファの内容を
* そのまま残す。mask は常にネイティブ解像度だが、このパスは【異なる】
* 解像度のレンダーターゲットへ描画している場合がある(PostProcessRenderer
* のDLSS-RRアップスケール後出力へ描くトーンマップ後デバッグオーバーレイ -
* Renderer::EndEditorFrame参照)ため、mask をインデックスする前に
* SV_Position を screen_size_/display_size_ でスケールし直す - 両者が
* 既に一致している場合は比率1.0の恒等変換になる。
*/
float4 main(SelectionOutlineOutput input) : SV_Target0
{
	Texture2D<float> mask = ResourceDescriptorHeap[structured_indices.model_.selection_mask_index_];
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	int2 pixel = int2(input.position.xy * (scene.screen_size_ / scene.display_size_));

	if (mask.Load(int3(pixel, 0)) > 0.5)
	{
		discard;
	}

	bool near_selected = false;
	for (int y = -OUTLINE_THICKNESS; y <= OUTLINE_THICKNESS && !near_selected; ++y)
	{
		for (int x = -OUTLINE_THICKNESS; x <= OUTLINE_THICKNESS; ++x)
		{
			if (mask.Load(int3(pixel + int2(x, y), 0)) > 0.5)
			{
				near_selected = true;
				break;
			}
		}
	}

	if (!near_selected)
	{
		discard;
	}

	return float4(OUTLINE_COLOR, 1.0);
}

#include "../../Shader/Structured.hlsli"

struct SelectionOutlineOutput
{
	float4 position : SV_Position;
	float2 texcoord : TEXCOORD0;
};

static const float3 OUTLINE_COLOR = float3(0.95, 0.55, 0.05);
static const int OUTLINE_THICKNESS = 2;

/**
* [JP]
* 選択アウトライン合成用ピクセルシェーダ。DrawSelectionMask が書いたマスク
* （選択メッシュ=1、それ以外=0）に対し、自身が選択メッシュの外側にあり、かつ
* 周囲 OUTLINE_THICKNESS ピクセル以内に選択メッシュがあるピクセルだけを
* 縁取り色で塗る。それ以外は discard してエディタフレームバッファの内容を
* そのまま残す。
*/
float4 main(SelectionOutlineOutput input) : SV_Target0
{
	Texture2D<float> mask = ResourceDescriptorHeap[structured_indices.model_.selection_mask_index_];

	int2 pixel = int2(input.position.xy);

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

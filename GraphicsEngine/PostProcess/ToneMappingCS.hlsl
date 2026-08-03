#include "../Shader/Constants.hlsli"
#include "../Shader/Sampler.hlsli"
#include "ToneMappingCurves.hlsli"

/**
* [EN]
* The actual display pass: reads the HDR scene color, applies exposure (the
* ExposureSettings.compensation_ EV always, plus the auto-exposure buffer's
* EV when enabled), applies the selected tone curve (or none - clamp only),
* and encodes to sRGB before writing into the UNORM texture the editor/game
* viewport actually shows. Runs unconditionally every frame regardless of
* ToneMappingSettings.enabled_/ExposureSettings.enabled_ - what those toggle
* is whether the CURVE and the AUTO EV apply, not whether this pass runs,
* because the sRGB encode below is not optional: the swapchain backbuffer is
* DXGI_FORMAT_R8G8B8A8_UNORM (no hardware sRGB view anywhere in this engine),
* so this is the only place a linear HDR value ever gets gamma-corrected for
* display. Before this pass existed the raw linear buffer was shown directly,
* which is why scenes without a bright emissive/sun disc always read as
* crushed and dark - see the exposure comment in PostProcess.h for the actual
* numbers.
*
* [JP]
* 実際の表示パス: HDR シーンカラーを読み、露出(ExposureSettings.compensation_
* の EV は常に、自動露出バッファの EV は有効時に加算)を適用し、選択された
* トーンカーブ(またはカーブ無し=クランプのみ)を適用し、エディタ/ゲーム
* ビューポートが実際に表示する UNORM テクスチャへ書く前に sRGB エンコードする。
* ToneMappingSettings.enabled_ / ExposureSettings.enabled_ に関わらず毎フレーム
* 無条件で走る — それらが切り替えるのは【カーブ】と【自動EV】の適用有無で
* あって、このパス自体の実行有無ではない。下の sRGB エンコードは省略できない
* からだ: スワップチェーンのバックバッファは DXGI_FORMAT_R8G8B8A8_UNORM
* (このエンジンにはハードウェア sRGB ビューがどこにも無い)なので、リニアな
* HDR 値がガンマ補正される場所はここが唯一。このパスが無かった頃は生のリニア
* バッファがそのまま表示されており、それが明るいエミッシブや太陽円盤の無い
* シーンが常に潰れて暗く見えていた理由 — 実数値は PostProcess.h の露出コメント
* 参照。
*/

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> output = ResourceDescriptorHeap[constant_indices.post_process_.output_uav_index_];

	uint width, height;
	output.GetDimensions(width, height);
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	// [JP] source_color_index_ は常に output と同じ解像度(DLSS-RR有効時は
	//      アップスケール後、無効時はネイティブ)を指すのでLoadで直接読める。
	//      一方 DepthOfFieldCS.hlsl(+BokehCS.hlsl)の出力バッファは常に
	//      ネイティブ解像度専用(PostProcessRenderer::View::depthOfFieldResource_
	//      のコメント参照) — DLSS-RR有効時は output の方がアップスケール後に
	//      なり解像度が食い違うため、Loadでピクセル座標のままアクセスすると
	//      ネイティブ解像度ぶんの領域しか埋まらず残りが黒くなる
	//      (「FrameBufferが1/8サイズになる」不具合の原因だった)。UV経由の
	//      SampleLevelなら解像度の違いを自動で吸収できるので、こちらだけ
	//      LensFlareCS.hlslの出力と同じUVサンプル方式にする。
	float3 hdr_color;
	if (constant_indices.post_process_.depth_of_field_.enabled_ != 0)
	{
		Texture2D<float4> depth_of_field_source = ResourceDescriptorHeap[constant_indices.post_process_.depth_of_field_.shader_resource_view_index_];
		float2 source_uv = (float2(dtid.xy) + 0.5) / float2(width, height);
		hdr_color = depth_of_field_source.SampleLevel(sampler_linear_clamp, source_uv, 0).rgb;
	}
	else
	{
		Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.post_process_.source_color_index_];
		hdr_color = source.Load(int3(dtid.xy, 0)).rgb;
	}

	// [JP] LensFlareCS.hlsl が1/4解像度で書いた加算フレアを、露出適用前の
	//      HDRカラーへバイリニアで足し込む(フレアも「露出される光」として
	//      扱う)。enabled_ が立っていない間はテクスチャに古いデータが
	//      残っている可能性があるため、サンプルごと丸ごとスキップする。
	if (constant_indices.post_process_.lens_flare_.enabled_ != 0)
	{
		Texture2D<float4> lens_flare = ResourceDescriptorHeap[constant_indices.post_process_.lens_flare_.shader_resource_view_index_];
		float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);
		hdr_color += lens_flare.SampleLevel(sampler_linear_clamp, uv, 0).rgb;
	}

	float exposure_ev = constant_indices.post_process_.exposure_.exposure_compensation_;
	if (constant_indices.post_process_.exposure_.auto_exposure_enabled_ != 0)
	{
		RWStructuredBuffer<float> exposure = ResourceDescriptorHeap[constant_indices.post_process_.exposure_.exposure_uav_index_];
		exposure_ev += exposure[0];
	}

	float3 color = hdr_color * exp2(exposure_ev);

	if (constant_indices.post_process_.tone_mapping_.tone_mapping_enabled_ != 0)
	{
		uint mode = constant_indices.post_process_.tone_mapping_.tone_mapping_mode_;
		if (mode == 1)
		{
			color = ReinhardToneMap(color);
		}
		else if (mode == 2)
		{
			color = AcesFilmicToneMap(color);
		}
		else if (mode == 3)
		{
			color = PbrNeutralToneMap(color);
		}
		// mode == 0 (None): カーブ無し、下の saturate だけがかかる。
	}

	color = saturate(color);
	color = LinearToSrgb(color);

	output[dtid.xy] = float4(color, 1.0);
}

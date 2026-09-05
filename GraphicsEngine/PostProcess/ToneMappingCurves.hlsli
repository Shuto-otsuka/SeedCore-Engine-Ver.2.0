#ifndef __TONE_MAPPING_CURVES_HLSL__
#define __TONE_MAPPING_CURVES_HLSL__

/**
* Reference (one per curve, each is a port of the source below):
* - https://www.cs.utah.edu/docs/techreports/2002/pdf/UUCS-02-001.pdf
*   (Reinhard et al. 2002, "Photographic Tone Reproduction for Digital
*   Images" - the basic Reinhard curve ReinhardToneMap below.)
* - https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
*   (Krzysztof Narkowicz's ACES filmic fit - AcesFilmicToneMap below.)
* - https://github.com/KhronosGroup/ToneMapping/tree/main/PBR_Neutral
*   (Khronos PBR Neutral - PbrNeutralToneMap below.)
* - https://en.wikipedia.org/wiki/SRGB
*   (piecewise sRGB OETF - LinearToSrgb below.)
*
* Standard published tone-mapping curves + the sRGB OETF, shared by
* ToneMappingCS.hlsl. Every curve here maps [0, inf) -> [0, 1] per channel;
* the caller is responsible for exposure (multiplying the HDR color before
* calling in) and for the final display encode (LinearToSrgb below).
*/

/**
* Simple per-channel Reinhard (x / (1+x)). No white-point parameter - this is
* the basic curve, not "Reinhard extended".
*/
float3 ReinhardToneMap(float3 color)
{
	return color / (1.0 + color);
}

/**
* Krzysztof Narkowicz's ACES filmic fit - the widely used analytic
* approximation of the ACES reference tonemapper's response, cheap enough for
* a full-screen pass every frame.
*/
float3 AcesFilmicToneMap(float3 color)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

/**
* Khronos PBR Neutral tone mapper (the glTF-Sample-Viewer reference
* implementation, ported to HLSL). Designed to stay closer to the source
* colors than ACES/Reinhard for mid-range values - it only starts compressing
* once a channel crosses start_compression, and desaturates rather than
* crushes as it approaches the peak, which is why UI/product-style renders
* often prefer it over ACES's stronger filmic S-curve.
*/
float3 PbrNeutralToneMap(float3 color)
{
	const float start_compression = 0.8 - 0.04;
	const float desaturation = 0.15;

	float min_channel = min(color.r, min(color.g, color.b));
	float offset = min_channel < 0.08 ? min_channel - 6.25 * min_channel * min_channel : 0.04;
	color -= offset;

	float peak = max(color.r, max(color.g, color.b));
	if (peak < start_compression)
	{
		return color;
	}

	float d = 1.0 - start_compression;
	float new_peak = 1.0 - d * d / (peak + d - start_compression);
	color *= new_peak / peak;

	float desaturation_amount = 1.0 - 1.0 / (desaturation * (peak - new_peak) + 1.0);
	return lerp(color, float3(new_peak, new_peak, new_peak), desaturation_amount);
}

/**
* Accurate piecewise sRGB OETF (not a plain pow(1/2.2) approximation). The
* swapchain backbuffer in this engine is DXGI_FORMAT_R8G8B8A8_UNORM, not an
* _SRGB format, so there is no hardware gamma conversion anywhere in the
* display path - this function IS the only gamma correction the image gets,
* which is why it must run unconditionally regardless of which tone curve
* (or none) was applied above.
*/
float3 LinearToSrgb(float3 linear_color)
{
	float3 lower_range = linear_color * 12.92;
	float3 higher_range = 1.055 * pow(max(linear_color, 0.0), 1.0 / 2.4) - 0.055;
	return lerp(lower_range, higher_range, step(0.0031308, linear_color));
}

#endif // __TONE_MAPPING_CURVES_HLSL__

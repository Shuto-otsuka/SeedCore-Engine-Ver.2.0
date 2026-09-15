#ifndef __WEATHER_HLSL__
#define __WEATHER_HLSL__

#include "../Shader/Constants.hlsli"

/**
* [EN]
* Weather state (WeatherSystem via LightSystem::Gather's weather
* parameter). Zero when the scene has no Weather component.
*/
struct WeatherConstantBuffer
{
	float wetness_;         // 0..1, wet-surface shading (rain + a drying-out tail).
	float snow_coverage_;   // 0..1, snow accumulation shading (builds up, melts over time).
	float thunder_flash_;   // 0..1, brief sky/ambient brightness burst on a lightning strike.
	float weather_padding_0_;

	float snow_intensity_;  // 0..1, "is it snowing right now" - drives the falling-snow screen overlay.
	float thunder_seed_;    // Re-rolled each strike - randomizes the lightning bolt shape.
	float2 weather_padding_1_;
};

ConstantBuffer<WeatherConstantBuffer> GetWeatherConstantBuffer()
{
	return ResourceDescriptorHeap[constant_indices.weather_index_];
}

#endif // __WEATHER_HLSL__

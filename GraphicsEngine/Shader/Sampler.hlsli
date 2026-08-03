#ifndef __SAMPLER_HLSL__
#define __SAMPLER_HLSL__

SamplerState sampler_point_wrap    : register(s0);
SamplerState sampler_point_clamp   : register(s1);
SamplerState sampler_point_mirror  : register(s2);
SamplerState sampler_linear_wrap   : register(s3);
SamplerState sampler_linear_clamp  : register(s4);
SamplerState sampler_linear_mirror : register(s5);
SamplerState sampler_aniso_wrap    : register(s6);
SamplerState sampler_aniso_clamp   : register(s7);
SamplerState sampler_aniso_mirror  : register(s8);
SamplerState sampler_border_black  : register(s9);
SamplerState sampler_border_white  : register(s10);

#endif // __SAMPLER_HLSL__
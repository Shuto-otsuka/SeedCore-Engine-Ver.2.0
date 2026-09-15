#ifndef __VERTEX_HLSL__
#define __VERTEX_HLSL__

#include "Normal.hlsli"

/**
* Mirrors the C++ CompressedVertex struct (Model/Crister.h) - 16 bytes. Used by
* the raytracing hit paths (reflection, global illumination, refraction, ambient
* occlusion, subsurface scattering), which only need the normal and texcoord
* recovered from a hit triangle. Model.hlsli's CompressedModelVertex is the
* same layout for the mesh-shader path.
*/
struct CompressedVertex
{
	uint position_xy_;
	uint position_z_texu_;
	uint texv_tangent_;
	uint normal_;
};

/**
* Decodes CompressedVertex's octahedral-packed normal_ field back into a
* world/object-space unit vector (Normal.hlsli: OctNormalDecode).
*/
float3 DecodeCompressedVertexNormal(CompressedVertex vertex)
{
	return OctNormalDecode(float2(vertex.normal_ & 0xFFFF, vertex.normal_ >> 16) / 65535.0);
}

/**
* Same unpacking as Model.hlsli's DecodeVertex - u is the high half of
* position_z_texu_, v the low half of texv_tangent_ - rescaled out of the
* mesh's UV AABB. Kept in sync with that: a mismatch here shows up as a
* raytraced effect sampling the wrong part of the texture.
*/
float2 DecodeCompressedVertexTexcoord(CompressedVertex vertex, float2 texcoord_min, float2 texcoord_extent)
{
	float2 texcoord01 = float2(vertex.position_z_texu_ >> 16, vertex.texv_tangent_ & 0xFFFF) / 65535.0;
	return texcoord_min + texcoord01 * texcoord_extent;
}

#endif // __VERTEX_HLSL__

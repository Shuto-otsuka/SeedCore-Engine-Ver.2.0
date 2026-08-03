/**
* [JP]
* レイトレ 屈折（RTPSO / DispatchRays）。ガラス・水などの透過面を通した先を出す。
* 一次面から視線を Snell の法則で屈折させたレイを撃ち、界面ごとに Fresnel で反射／屈折に
* 分岐しながら数バウンス辿る。KHR_materials_ior / transmission / volume と連携する:
*   - ior      : 屈折率（Snell）
*   - transmission : 透過の重み
*   - volume(attenuation) : 媒質内の吸収（Beer-Lambert）
* closesthit から再帰的に TraceRay するため、RaytracingStateKey の maxTraceRecursionDepth_ を
* バウンス数ぶん確保すること（例: 4）。1spp のノイズは DLSS-RR が均す。
*
* 方式選択の理由: 界面での分岐・媒質内吸収・再帰追跡が要り、ヒット面のマテリアルを
* closesthit で評価する必要があるので RTPSO 一択。
*
* C++側(B): raygen/miss/closesthit を登録し maxTraceRecursionDepth_ を上げる。DispatchRays。
*/

// radiance_(12)+throughput...は 16byte を超えるので payload を少し広げる。
// radiance_(12)+bounce_(4)+throughput_(12)+hit_distance_(4)=32 byte。
// RaytracingStateKey.maxPayloadSizeInBytes_ を 32 に設定すること。
struct RefractionPayload
{
	float3 radiance_;
	uint bounce_;                     // 現在のバウンス数（再帰の停止条件）
	float3 throughput_;               // ここまでの減衰（Fresnel×吸収の積）
	float hit_distance_;
};

struct RefractionConstantBuffer
{
	uint tlas_index_;
	uint depth_index_;
	uint normal_index_;
	uint output_index_;

	uint sky_environment_cube_index_;
	uint frame_index_;
	float ray_t_max_;
	float normal_bias_;

	uint max_bounces_;                // 追跡する界面の最大数
	float3 refraction_padding_;

	row_major float4x4 inverse_view_projection_;

	float3 camera_position_;
	float refraction_padding_1_;

	float2 screen_size_;
	float2 inverse_screen_size_;
};
ConstantBuffer<RefractionConstantBuffer> refraction : register(b0, space1);

[shader("raygeneration")]
void RefractionRayGeneration()
{
	uint2 pixel = DispatchRaysIndex().xy;
	RWTexture2D<float4> output = ResourceDescriptorHeap[refraction.output_index_];

	Texture2D<float> depth_texture = ResourceDescriptorHeap[refraction.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));
	if (depth == 0.0)
	{
		output[pixel] = float4(0, 0, 0, 1);
		return;
	}

	// ワールド座標と法線を復元。
	float2 uv = (float2(pixel) + 0.5) * refraction.inverse_screen_size_;
	float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
	float4 clip = float4(ndc, depth, 1.0);
	float4 world = mul(clip, refraction.inverse_view_projection_);
	float3 world_position = world.xyz / world.w;

	Texture2D<float3> normal_texture = ResourceDescriptorHeap[refraction.normal_index_];
	float3 normal = normalize(normal_texture.Load(int3(pixel, 0)));

	// ---- TODO 1: 視線方向 view_dir = normalize(world_position - camera_position_)。

	// ---- TODO 2: payload を初期化する。radiance_=0 / bounce_=0 / throughput_=1 / hit_distance_=0。

	// ---- TODO 3: 一次面から最初の屈折レイを撃つ。origin=world_position - normal*normal_bias_
	//              (面の裏側へ入るので法線と逆に押し出す)、direction=view_dir をそのまま or
	//              界面で屈折させた方向。TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload)。

	// ---- TODO 4: payload.radiance_ を output[pixel] に書く。
	output[pixel] = float4(0, 0, 0, 1); // 仮。
}

[shader("miss")]
void RefractionMiss(inout RefractionPayload payload)
{
	// ---- TODO 5: 空へ抜けた。環境キューブをサンプルし、throughput_ を掛けて radiance_ に加算。
	//              payload.radiance_ += payload.throughput_ * sky。hit_distance_ に遠方値。
}

[shader("closesthit")]
void RefractionClosestHit(inout RefractionPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
	// ---- TODO 6: 界面処理。
	//   6a: ヒット面の法線・ior・transmission を bindless から引く（InstanceID/PrimitiveIndex/bary）。
	//   6b: 入射か出射かを dot(WorldRayDirection(), N) の符号で判定し、eta = ior_out/ior_in を決める。
	//   6c: Fresnel(Schlick) で反射率 F を求め、屈折方向を refract() で計算（全反射なら反射のみ）。
	//   6d: throughput_ に (1-F)×transmission（屈折側）を掛ける。媒質を通った距離ぶん
	//       Beer-Lambert 吸収 exp(-attenuation * RayTCurrent()) を掛ける。
	//   6e: bounce_+1 が max_bounces_ 未満なら、屈折方向へ再帰 TraceRay。超えたら打ち切り。
	//       ヒット点 = WorldRayOrigin() + WorldRayDirection() * RayTCurrent()。
	//       payload.hit_distance_ = RayTCurrent()。
	//   まずは「屈折させず素通しで裏側の環境を出す」ところから疎通確認してよい。
}

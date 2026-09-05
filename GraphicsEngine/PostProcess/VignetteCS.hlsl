#include "../Shader/Constants.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [EN]
* Reference:
* - https://www.cs.cmu.edu/~sensing-sensors/readings/vignetting.pdf
* - https://en.wikipedia.org/wiki/Vignetting
*
* Natural vignetting: the cosine-fourth law. Illumination at the sensor
* falls off as cos^4 of the angle off the optical axis, a consequence of the
* projected areas of the pupil and the pixel combined with the inverse
* square law. Unlike optical vignetting (lens elements shading each other,
* which stopping down cures) or mechanical vignetting (a hood clipping the
* corners), this one is intrinsic to every lens including an ideal one, so
* it is the kind worth modelling as the default.
*
* exponent_ defaults to 4 for exactly that reason - that value IS the
* physical law. Moving it is art direction, so it is a slider rather than a
* constant, but 4 is where it belongs if the goal is a plausible lens.
*
* This pass runs BEFORE exposure and the tone curve. Vignetting is light
* that never reached the sensor, so it has to be part of what gets exposed;
* darkening the already-exposed, already-tone-mapped image instead would
* respond quite differently to the curve and would not survive auto
* exposure sensibly.
*
* Reading and writing the same texture is safe here and is what the
* renderer does when chromatic aberration ran first: this is a pure
* per-pixel multiply with no neighbour taps, so there is no read-write
* hazard (the same reason BokehCS.hlsl may read-modify-write depth of
* field's buffer).
*
* ---------------------------------------------------------------------
*
* [JP]
* 自然ビネット: コサイン4乗則。センサー面の照度は光軸からの角度の cos^4 で
* 落ちる。瞳と画素の投影面積、および逆二乗則の帰結。光学ビネット(素子同士の
* 遮蔽。絞れば解消する)や機械ビネット(フードによる四隅のケラレ)と違い、
* これは理想レンズを含むあらゆるレンズに内在するので、既定でモデル化する
* 価値があるのはこちら。
*
* exponent_ の既定値が4なのはまさにその理由で、この値が【物理法則そのもの】。
* 動かすのはアートディレクションなので定数ではなくスライダーにしてあるが、
* もっともらしいレンズを目指すなら4が本来の位置。
*
* このパスは露出とトーンカーブより【前】に走る。ビネットは「そもそも
* センサーへ届かなかった光」なので、露出される対象そのものに含まれて
* いなければならない。露出もトーンマップも済んだ画像を後から暗くすると、
* カーブへの応答がまるで変わるし、自動露出とも噛み合わない。
*
* 同じテクスチャを読み書きしても安全で、色収差が先に走った場合に
* レンダラーが実際そうする: これは近傍タップの無い画素ごとの単純な乗算
* なので読み書きの競合が起きない(BokehCS.hlsl が被写界深度のバッファを
* read-modify-write できるのと同じ理由)。
*/
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> destination = ResourceDescriptorHeap[constant_indices.post_process_.vignette_.destination_uav_index_];

	uint width, height;
	destination.GetDimensions(width, height);

	/// [EN] Bounds guard: the dispatch is rounded up to a multiple of the
	///      8x8 thread group size, so threads past the actual screen edge
	///      must bail out before touching any resource.
	/// [JP] 範囲外ガード: ディスパッチは 8x8 スレッドグループの倍数に
	///      切り上げられているので、実際の画面端を超えたスレッドはどの
	///      リソースにも触れる前に抜ける必要がある。
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.post_process_.vignette_.source_srv_index_];

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);
	float3 color = source.SampleLevel(sampler_linear_clamp, uv, 0).rgb;

	float intensity = constant_indices.post_process_.vignette_.intensity_;
	float exponent = constant_indices.post_process_.vignette_.exponent_;
	float3 vignette_color = constant_indices.post_process_.vignette_.color_.rgb;

	/// [EN] Aspect-corrected distance from center. Without the correction,
	///      the darkening comes out as a horizontally stretched ellipse,
	///      breaking the illusion of a phenomenon coming from the lens's
	///      circular pupil.
	/// [JP] アスペクト補正した中心からの距離。補正しないと減光が横長の
	///      楕円になり、レンズの円形の瞳から来る現象として破綻する。
	float aspect = float(width) / float(height);
	float2 centered = (uv - 0.5) * float2(aspect, 1.0);
	float radius = length(centered) / length(float2(0.5 * aspect, 0.5));

	/// [EN] The cos^4 law. Maps radius to an angle off the optical axis, then
	///      raises that angle's cosine to the exponent_ power. 1 at
	///      radius=0 (on-axis), minimal at the corners.
	///
	///      Mapping via atan bakes in a fixed assumption: radius=1
	///      corresponds to a 45-degree half-angle (a 90-degree-FOV lens).
	///      Strictly, the angle should be derived from the camera's actual
	///      field of view, but that would make the vignette's strength
	///      change just from changing FOV. This is kept fixed on the
	///      assumption that intensity_ is the knob for tuning it instead.
	/// [JP] cos^4 則。半径を光軸からの角度へ写し、その余弦を exponent_ 乗
	///      する。radius=0(光軸上)で1、四隅で最小。
	///
	///      atan で写しているので radius=1 が半画角45度(画角90度のレンズ)
	///      相当になる、という固定の仮定を置いている。厳密にやるなら
	///      実際のカメラの画角から角度を出すべきだが、そうするとFOVを
	///      変えただけでビネットの強さが変わることになる。ここは
	///      intensity_ で調整する前提で固定にしている。
	float angle = atan(radius);
	float falloff = pow(saturate(cos(angle)), max(exponent, 0.0));

	/// [EN] Uses intensity_ as the exponent on falloff. intensity=1 is the
	///      cosine-fourth law as-is (falloff^1 = falloff), intensity=0
	///      disables it (falloff^0 = 1), and above 1 falloff (a value in
	///      [0,1)) shrinks further, darkening the corners beyond the
	///      physical curve. Because falloff is clamped by atan to a
	///      45-degree half-angle equivalent, it never reaches exactly 0 even
	///      at the corners (it is cos(45deg)~=0.707 raised to exponent_
	///      instead), so this never touches the undefined 0^0 region.
	/// [JP] intensity_ を falloff の指数として使う。intensity=1 でコサイン4乗則
	///      そのもの(falloff^1 = falloff)、intensity=0 で無効(falloff^0 = 1)、
	///      1を超えると falloff([0,1)の値)がさらに小さくなるので、物理カーブ
	///      より四隅が暗くなる。falloff は atan で半画角45度相当にクランプして
	///      あるため四隅でも厳密な0にはならず(cos(45°)≈0.707からexponent_乗した
	///      値)、0^0 のような未定義域には触れない。
	float blend = pow(saturate(falloff), max(intensity, 0.0));

	destination[dtid.xy] = float4(lerp(vignette_color, color, blend), 1.0);
}

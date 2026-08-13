#include "../Shader/Constants.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [EN]
* Reference:
* - https://www.tangramvision.com/blog/the-innovative-brown-conrady-model
* - https://vitalyvorobyev.github.io/calibration-rs/distortion.html
*
* Radial lens distortion, the radial half of the Brown-Conrady model: a
* point at radius r from the optical axis is displaced to
* r * (1 + k1*r^2 + k2*r^4 + k3*r^6). k1 dominates the look, k2 refines the
* corners, k3 barely moves anything. Positive coefficients bow the image
* outward (barrel/fisheye), negative pull it inward (pincushion).
*
* Direction matters and is a classic source of confusion. Camera
* calibration usually solves the FORWARD problem: given a real-world ray,
* where does it land on the sensor. A post-process runs the other way -
* this shader iterates over OUTPUT pixels and has to answer "where in the
* source image does this pixel come from", so it applies the polynomial to
* the destination UV to produce the sample UV. That is why no iterative
* inversion appears here: for this direction the polynomial is used
* directly, not inverted.
*
* Brown-Conrady also has TANGENTIAL terms (p1, p2) modelling elements that
* are decentred or tilted relative to the axis. They are deliberately not
* implemented: that is a manufacturing defect rather than a look, and the
* asymmetric skew it produces mostly reads as a mistake.
*
* scale_ zooms in before distorting. Barrel distortion pulls the image away
* from the corners and leaves them empty, so without this a positive k1
* shows black corners; the clamp sampler would otherwise smear the edge
* pixels outward instead.
*
* ---------------------------------------------------------------------
*
* [JP]
* 半径方向のレンズ歪曲。Brown-Conrady モデルの半径方向の項で、光軸から
* 半径 r の点が r * (1 + k1*r^2 + k2*r^4 + k3*r^6) へ変位する。見た目を
* 支配するのは k1、k2 が四隅を微調整し、k3 はほとんど動かさない。係数が
* 正で外側へ膨らみ(樽型/魚眼)、負で内側へ引き込まれる(糸巻き型)。
*
* 【向き】が重要で、ここは典型的な混乱の元。カメラキャリブレーションが
* 解くのは普通【順方向】の問題 — 実世界の光線がセンサー上のどこに落ちるか。
* ポストプロセスはその逆で、このシェーダは【出力】画素を走査し「この画素は
* ソース画像のどこから来るか」に答える必要がある。だから多項式は出力UVに
* 適用してサンプルUVを作る。反復的な逆変換がどこにも出てこないのはそのため:
* この向きでは多項式をそのまま使うのであって、逆にする必要がない。
*
* Brown-Conrady には【接線方向】の項(p1, p2)もあり、光軸に対して偏心・
* 傾斜した素子をモデル化する。意図的に実装していない: 製造上の欠陥であって
* 画作りではなく、生じる非対称な歪みは大抵単なる失敗に見えるため。
*
* scale_ は歪ませる前の拡大。樽型歪曲は像を四隅から引き離して余白を作るので、
* これが無いと k1 が正のとき四隅が黒くなる(正確にはクランプサンプラが端の
* 画素を外側へ引き伸ばした状態になる)。
*/
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture2D<float4> destination = ResourceDescriptorHeap[constant_indices.post_process_.lens_distortion_.destination_uav_index_];

	uint width, height;
	destination.GetDimensions(width, height);
	if (dtid.x >= width || dtid.y >= height)
	{
		return;
	}

	Texture2D<float4> source = ResourceDescriptorHeap[constant_indices.post_process_.lens_distortion_.source_srv_index_];

	float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);

	float k1 = constant_indices.post_process_.lens_distortion_.k1_;
	float k2 = constant_indices.post_process_.lens_distortion_.k2_;
	float k3 = constant_indices.post_process_.lens_distortion_.k3_;
	float scale = max(constant_indices.post_process_.lens_distortion_.scale_, 0.0001);

	/// [JP] 半径はアスペクト補正した空間で測る。補正しないと歪曲が横長に
	///      なり、円形の瞳を通した歪みとして破綻する。補正は測る時だけで、
	///      オフセットはUV空間へ戻してから足す。
	float aspect = float(width) / float(height);
	float2 centered = (uv - 0.5) * float2(aspect, 1.0);

	float radius_squared = dot(centered, centered);
	float radius_fourth = radius_squared * radius_squared;
	float radius_sixth = radius_fourth * radius_squared;

	float distortion = 1.0 + k1 * radius_squared + k2 * radius_fourth + k3 * radius_sixth;

	float2 distorted = centered * distortion / scale;
	float2 sample_uv = distorted / float2(aspect, 1.0) + 0.5;

	destination[dtid.xy] = float4(source.SampleLevel(sampler_linear_clamp, sample_uv, 0).rgb, 1.0);
}

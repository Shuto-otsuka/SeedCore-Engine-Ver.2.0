#ifndef __CLOUD_NOISE_BAKE_HLSL__
#define __CLOUD_NOISE_BAKE_HLSL__

// Tileable Worley (cellular) + Perlin (gradient) noise for baking cloud
// density fields into wrap-sampled Texture3Ds. The regular Noise.hlsli
// generators are NOT periodic, so they would seam when the texture wraps -
// here the lattice is wrapped modulo the cell count, making every octave
// exactly tileable.

/// Deterministic float3 in [0,1)^3 from a lattice cell.
float3 CloudCellHash(float3 cell)
{
	float3 p = float3(
		dot(cell, float3(127.1, 311.7, 74.7)),
		dot(cell, float3(269.5, 183.3, 246.1)),
		dot(cell, float3(113.5, 271.9, 124.6)));
	return frac(sin(p) * 43758.5453123);
}

/**
* Saturating linear remap - the standard combining operator for cloud noise.
* Maps [low, high] onto [new_low, new_high] and clamps outside that window.
*/
float CloudRemap(float value, float low, float high, float new_low, float new_high)
{
	return new_low + saturate((value - low) / max(high - low, 0.0001)) * (new_high - new_low);
}

// ---------------------------------------------------------------------------
// Worley (cellular)
// ---------------------------------------------------------------------------

/**
* Tileable Worley noise. p in [0,1)^3, cells = lattice resolution (the tiling
* period, must be an integer). Returns distance to the nearest feature point,
* roughly [0,1].
*/
float TileableWorley(float3 p, float cells)
{
	float3 grid_position = p * cells;
	float3 base_cell = floor(grid_position);
	float3 fraction = grid_position - base_cell;

	float min_distance_squared = 1e5;

	[unroll]
	for (int z = -1; z <= 1; z++)
	{
		[unroll]
		for (int y = -1; y <= 1; y++)
		{
			[unroll]
			for (int x = -1; x <= 1; x++)
			{
				float3 offset = float3(x, y, z);
				float3 wrapped_cell = fmod(base_cell + offset + cells, cells);
				float3 feature = CloudCellHash(wrapped_cell);
				float3 delta = offset + feature - fraction;
				min_distance_squared = min(min_distance_squared, dot(delta, delta));
			}
		}
	}

	return sqrt(min_distance_squared);
}

/**
* Inverted-Worley FBM ("billow"): bright blobs on a dark background - the
* puffy cauliflower component of a cumulus. base_cells doubles per octave so
* every octave stays tileable.
*
* Keep (base_cells << octaves) at or below a quarter of the baked texture
* resolution: the top octave needs about four voxels per cell, otherwise it
* bakes to aliased hash noise rather than usable detail.
*/
float TileableWorleyFbm(float3 p, float base_cells, int octaves)
{
	float total = 0.0;
	float amplitude = 0.5;
	float amplitude_sum = 0.0;
	float cells = base_cells;

	for (int octave = 0; octave < octaves; octave++)
	{
		total += (1.0 - saturate(TileableWorley(p, cells))) * amplitude;
		amplitude_sum += amplitude;
		amplitude *= 0.5;
		cells *= 2.0;
	}

	return total / max(amplitude_sum, 0.0001);
}

// ---------------------------------------------------------------------------
// Perlin (gradient)
// ---------------------------------------------------------------------------

/// Pseudo-random unit vector for a lattice cell. rsqrt is guarded because the
/// hash can land arbitrarily close to the centre of the [-1,1] cube.
float3 CloudCellGradient(float3 cell)
{
	float3 v = CloudCellHash(cell) * 2.0 - 1.0;
	return v * rsqrt(max(dot(v, v), 0.0001));
}

/**
* Tileable Perlin gradient noise, signed and roughly in [-0.7, 0.7].
* Same wrapped lattice as TileableWorley, with the usual quintic fade.
*/
float TileablePerlin(float3 p, float cells)
{
	float3 grid_position = p * cells;
	float3 base_cell = floor(grid_position);
	float3 fraction = grid_position - base_cell;
	float3 fade = fraction * fraction * fraction * (fraction * (fraction * 6.0 - 15.0) + 10.0);

	float total = 0.0;

	[unroll]
	for (int z = 0; z <= 1; z++)
	{
		[unroll]
		for (int y = 0; y <= 1; y++)
		{
			[unroll]
			for (int x = 0; x <= 1; x++)
			{
				float3 offset = float3(x, y, z);
				float3 wrapped_cell = fmod(base_cell + offset + cells, cells);
				float value = dot(CloudCellGradient(wrapped_cell), fraction - offset);
				float3 blend = lerp(1.0 - fade, fade, offset);
				total += value * blend.x * blend.y * blend.z;
			}
		}
	}

	return total;
}

/// Perlin FBM normalised into roughly [0,1] (mean near 0.5).
float TileablePerlinFbm(float3 p, float base_cells, int octaves)
{
	float total = 0.0;
	float amplitude = 0.5;
	float amplitude_sum = 0.0;
	float cells = base_cells;

	for (int octave = 0; octave < octaves; octave++)
	{
		total += TileablePerlin(p, cells) * amplitude;
		amplitude_sum += amplitude;
		amplitude *= 0.5;
		cells *= 2.0;
	}

	return saturate(total / max(amplitude_sum, 0.0001) * 0.7 + 0.5);
}

// ---------------------------------------------------------------------------
// Combined cloud base
// ---------------------------------------------------------------------------

/**
* Perlin-Worley cloud base. Inverted Worley alone reads as uniform foam or
* bubble wrap - every blob the same size, no connected structure. Perlin
* supplies the large-scale billowing body that ties blobs into masses.
*
* The Perlin field is remapped into a multiplier centred on 1.0 before it
* modulates the Worley field, so the product keeps the Worley field's mean.
* That matters: CloudDensity's coverage remap is calibrated against this
* field's average, so shifting the mean here would silently recalibrate the
* coverage_ slider.
*/
float TileablePerlinWorley(float3 p, float base_cells, int octaves)
{
	float worley = TileableWorleyFbm(p, base_cells, octaves);
	float perlin = TileablePerlinFbm(p, base_cells, octaves);

	return saturate(worley * CloudRemap(perlin, 0.25, 0.75, 0.55, 1.45));
}

#endif // __CLOUD_NOISE_BAKE_HLSL__

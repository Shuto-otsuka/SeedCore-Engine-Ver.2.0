# SeedHuman.hc — file layout (format v2)

Container: `[16-byte IV][ AES-256-CBC(PKCS7) ciphertext ]`, identical scheme to `.icon`.
Key = `SHA256(SC_ENCRYPTION_KEY_SEED)` (32 bytes). Decrypt with `SeedCore::Aes256::Decrypt(key, iv, cipher)` → strips PKCS7 → the blob below.

All little-endian. No strings anywhere. Axis names / bone names are positional —
hardcoded `humanCharacterAxisNames[]` / `humanCharacterBoneNames[]` in the module,
index = array position.

## Source pipeline

Built by a throwaway converter (scratchpad `mh_convert.py`) from MakeHuman CC0 data:
`base.obj` + `data/targets/*.target` + `rigs/default.mhskel` + `default_weights.mhw`.
The converter reads the ready-made `.target` files directly (never diffs whole
exported meshes) so MakeHuman's authored per-region falloff is preserved — no
tearing at ±1. Regenerate:

```
git clone --filter=blob:none --no-checkout https://github.com/makehumancommunity/makehuman.git mh
cd mh && git sparse-checkout set --no-cone makehuman/data/targets makehuman/data/3dobjs makehuman/data/rigs && git checkout
py mh_convert.py
```

## Blob

```
HEADER  (offset 0, 17 × u32)
  u32 magic        = 0x43485353
  u32 version      = 2
  u32 vertexCount      NT        // body + aux joint verts
  u32 triangleCount    TRI
  u32 axisCount        K         // 33
  u32 boneCount        B         // 163
  u32 off[10]                    // byte offsets from blob start, block order below
  u32 fileSize                   // == decrypted blob length
```

| # | block    | contents |
|---|----------|----------|
| 0 | pos      | f32[NT*3]  neutral positions (engine space: LH, metres, Y-up, origin at MakeHuman root joint height). Neutral macro state = androgynous young caucasian (`base + ½·caucasian-female-young + ½·caucasian-male-young`). |
| 1 | nrm      | f32[NT*3]  smooth normals of the neutral mesh (recompute after deform) |
| 2 | uv       | f32[NT*2]  per-vertex UV, V flipped for engine. aux verts get (0,0) |
| 3 | tris     | u32[TRI*3] triangle indices, engine (LH) winding — `(p1-p0)×(p2-p0)` is outward. Reference body verts only (0..bodyVertexCount-1). |
| 4 | region   | u8[NT]     0 = body skin (rendered), 255 = aux joint vert (regressor only) |
| 5 | axes     | see below |
| 6 | sw       | f32[NT*4]  skin weights (normalised, sorted desc) |
| 7 | si       | u32[NT*4]  skin bone indices (into boneNames[] order) |
| 8 | bones    | B × { i32 parentIndex(-1=root); f32 restHead[3]; f32 restTail[3] } — stride 28, ordered to match `humanCharacterBoneNames[]`, index 0 = "root" |
| 9 | jgroups  | B × { u32 count; u32 vertexIndex[count] } — joint regressor: bone head = mean of these verts' (deformed) positions. From `default.mhskel` `joints["<bone>____head"]`. |

### axes block (v2 — paired)

```
K × { f32 defaultValue; f32 minValue; f32 maxValue }          // meta table, axis order
then K × {                                                     // paired sparse displacement
  u32 positiveCount
  u32 positiveVertexIndex[positiveCount]     // ascending
  f32 positiveDelta[positiveCount*3]         // metres, engine space, per-unit (applied for weight > 0)
  u32 negativeCount
  u32 negativeVertexIndex[negativeCount]
  f32 negativeDelta[negativeCount*3]         // applied (× -weight) for weight < 0
}
```

Positive and negative halves are **independent target files** with their own falloff
(e.g. `measure-waist-circ-incr` vs `-decr`), not negatives of each other.

### axis → MakeHuman target mapping

| axis | + (weight > 0) | − (weight < 0) |
|------|----------------|----------------|
| Gender | ½(caucasian-male-young − caucasian-female-young) | mirror |
| Age | ½(cauc-*-old) − neutral | ½(cauc-*-child) − neutral |
| Muscle | ½(univ-*-young-maxmuscle-avgweight) | …minmuscle |
| Weight | ½(univ-*-young-avgmuscle-maxweight) | …minweight |
| Height | ½(height/*-maxheight), range ±0.7 | …minheight |
| Measure_* (20) | `measure-<name>-incr.target` | `measure-<name>-decr.target` |
| Head_ScaleWide/Tall/Deep | `head-scale-{horiz,vert,depth}-incr` | `-decr` |
| Head_Age / Head_Fat | `head-{age,fat}-incr` | `-decr` |
| Nose_Size | Σ `nose-scale-{vert,horiz,depth}-incr` | `-decr` |
| Mouth_Wide | `mouth-scale-horiz-incr` | `-decr` |
| Chin_Prominent | `chin-prominent-incr` | `-decr` |

Length/height measure axes (upperarm/lowerarm/upperleg/lowerleg/napetowaist/waisttohip)
carry range ±0.6 to keep the near-rigid limb translations from over-extrapolating.

## Forward pass (character-creator eval / bake)

```
1. v[i] = pos[i] + Σ_axis  ( β>0 ? β · positiveDelta[axis][i] : -β · negativeDelta[axis][i] )
2. for each bone b:  head_b = mean( v[ jgroups[b] ] )
3. LBS (bake only): out[i] = Σ_k sw[i][k] · ( worldPose(si[i][k]) · v[i] )  — A-pose ⇒ identity in-editor
4. recompute normals from the deformed triangles
5. bake → glTF via HumanCharacterConverter (drop verts ≥ bodyVertexCount, region, jgroups)
```

## Known v1 limitations (data side)

- No pose blendshapes → A-pose only. `B_P(θ)` omitted.
- Eyes / teeth / tongue / eyelashes are MakeHuman helper geometry, not included —
  body skin only. Their bones exist in the rig (unskinned, joint positions still
  regressed) for later.
- Age −1 = full MakeHuman child (large delta); Height ±0.7 ≈ ±0.5 m.

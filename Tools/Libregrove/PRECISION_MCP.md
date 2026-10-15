# Libregrove precision MCP extension

The user authorized extending the Blender MCP on 2026-09-08 after the first-pass
capability-gap handoff. This is a typed stdio MCP server, not a generic Python or
Blender console endpoint. Run it with the existing MCP SDK environment:

```powershell
Saved/CindehearthTripoEnv/Scripts/python.exe Tools/Libregrove/precision_mcp.py
```

`precision_workflow.py` and `precision_stage.py` are MCP clients only. Blender
implementation lives in `precision_worker.py`, launched exclusively from a fixed
command manifest by `precision_mcp.py`. No expressions, code, arbitrary operators,
or external executable names can be supplied in requests.

Inputs are immutable `.blend` files beneath the Libregrove exchange directory.
Writes require a NEW object and path under `precision_v2`, a matching source
fingerprint, and a stable job ID. Durable requests/results live in
`Saved/LibregrovePrecision`. Interrupted jobs must be polled, not blindly retried.
Input-file hashes and source-mesh fingerprints are compared before saving.

Implemented capabilities: selected planar dissolve, selected nonmanifold-edge
splitting, guarded component symmetry, explicit seam flags, per-edge sharpness,
native fractional-margin UV packing, selected triangular UV reprojection,
closed backing panels/discs, deterministic triangulation with strictly bounded
tiny-face cleanup, exact topology/UV overlap inspection, bidirectional sampled
surface comparison, and clean review assembly. Existing typed repair MCP handles
explicit exports and isolated review renders.

The implementation follows the installed Blender 5.0 runtime and was tested there.
Reference contracts: [Blender BMesh operators](https://docs.blender.org/api/5.0/bmesh.ops.html)
and [Blender UV operators](https://docs.blender.org/api/main/bpy.ops.uv.html).
Live MCP schemas and synthetic test receipts are retained under
`Docs/Libregrove/BlenderQA_20260908/precision_v2` and its parent `evidence` directory.

Important findings:

- Whole-object decimation damaged shingles/windows in the first pass; those
  candidates remain rejected. Planar dissolve preserves remaining vertex positions.
- Splitting an edge used by more than two faces resolves ambiguous sheets; it can
  increase Blender vertex count and boundary count without increasing triangle count.
- Triangulating n-gons exposed tiny degenerate triangles and a few new shared
  diagonals; those require post-triangulation inspection/cleanup.
- Reprojection once wrote very small UV triangles at a large temporary offset.
  Float32 storage erased their altitude. Direct tile-zero assignment fixed this;
  the final exported GLB UV data is audited independently as well.
- UV area is the sum of triangle areas, not a continuous union proof. Positive
  overlaps use exact 2D triangle clipping with documented float32 tolerances.
- Packing margin is a 0.001 final-UV fraction, not a guaranteed 4-pixel gutter at
  every texture resolution. Texture baking/dilation/mipmap QA remains separate.
- Open boundaries in layered architecture are not globally watertight geometry.
  A zero nonmanifold-edge count is not a claim that every boundary is sealed.
- No Unreal imports, LOD generation, collision/lightmap authoring, texture painting,
engine performance certification, or paid Tripo calls are part of this pass.

Final roundtrip verification added a read-only, hash-guarded FBX import inspector
in a disposable Blender process. FBX validation exposed duplicate polygons that
the importer discarded. Exact coincident triangle removal now validates equal
coordinates/materials; opposite-winding duplicates require an existing two-sided
material. Exact UV float32 V-flip auditing exposed a workshop sliver that passed
Blender/FBX but collapsed in GLB. A bounded <=1e-5 UV-corner shift, with no global
repacking and mandatory overlap revalidation, repairs the evidenced sliver.
Synthetic suite 005 tests these two final capabilities before user-mesh edits.

## V3 texturing handoff, 2026-09-08

New bounded tools: surface-error/AABB-guarded native collapse decimation;
translation of exact complete components by <=5mm; deletion of exact zero-face
wire edges only. Synthetic `V3_PRECISION_TEST_002` passed before the new contact
operations. These remain fixed typed commands, not user-programmable scripts.
V3 uses 0.002 UV margin and preserves all prior V2 artifacts.

Six assets accepted at about 20% triangle reduction, after sharp-normal rebuild.
35% cottage/inn trials and the additional 20% tree trial were rejected by the
surface-error guard. Cottage front roof black shading was caused by a folded
internal triangle; workshop window and mill beam black shading came from
coplanar overlapping parts (not simply holes). Exact removal / 2mm contact
separation resolved the observed problem pixels on new copies. Some other
roof/ledge gaps remain, explicitly documented in the delivery.

Authoritative evidence: `Docs/Libregrove/BlenderQA_V3_20260908`; controllers are
`polish_v3.py`, `polish_repairs.py`, `polish_uv_finish.py`, `polish_delivery.py`.
The deliverable `Docs/Libregrove/Export_Texturing_V3_20260908` has nine standalone
clean sources, FBX/GLB exports, UV diagrams, final previews and QA. Texture-free,
no engine/LOD/collision work. V3 models retain game-world cm scale, not real-human
architecture. Original `mAP.blend` remains untouched; do not save the live source.

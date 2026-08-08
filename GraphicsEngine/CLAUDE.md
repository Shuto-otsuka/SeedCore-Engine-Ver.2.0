# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

SeedCore is a Windows C++ game engine (DirectX 12) split into a set of Visual Studio projects. Everything is built through the `Runtime/Runtime.sln` solution — there is no CMake/Make/CLI build system, so building, running, and debugging happen through MSBuild/Visual Studio.

## Common commands

- **Build**: open and build `Runtime/Runtime.sln` (Visual Studio, x64, Debug or Release), or from a Developer Command Prompt:
  `msbuild Runtime/Runtime.sln /p:Configuration=Debug /p:Platform=x64`
- **Run the game runtime**: build/run the `Runtime` project (entry point `Runtime/Application/Main.cpp`, `WinMain`).
- **Run the editor**: build/run the `Editor` project (entry point `Editor/Editor/Main.cpp`).
- **Clean generated/codegen artifacts**: `UserProject/Clean.bat` (wraps `Tools/Python/Clean.py`) — clears `UserProject/Assets`, `Scene`, `Prefab`, `SourceCode`, and strips the auto-generated blocks out of `ReflectionRegistry.cpp` / `PayloadRegistry.cpp`.
- **First-time setup**: `UserProject/Startup.bat` checks for a `py` (Python) install and offers to install it — Python must be on `PATH` as `py` before building, since codegen runs as an MSBuild pre-build step.
- There are no unit tests in this repo; there is no lint/test command to run.
- **Do not edit `.vcxproj`/`.vcxproj.filters` files.** New files are registered into the project by the user, not by Claude — when a new source file is added, leave it out of the `.vcxproj`; don't add `<ClCompile>`/`<ClInclude>` entries yourself.
- **Do not build the project to verify changes.** The user builds/checks compilation themselves — don't run `msbuild` (or otherwise attempt a build) just to confirm code compiles.
- **Never run `git commit` (or `git add`+`git commit`) — the user always commits themselves.** Leave changes staged/unstaged in the working tree; don't even ask for confirmation before committing, just leave it.

## Code generation (important — read before editing components/payloads)

`FoundationEngine`'s pre-build event runs two Python scripts against the whole project tree (`Tools/Python/Reflection.py` and `Tools/Python/Payload.py`, invoked with the repo root as argument). These scripts scan C++ source for annotation macros and regenerate code inside marked regions:

- `SC_REFLECTION_FIELD()`, `SC_REFLECTION_FIELD_EX("name")`, `SC_REFLECTION_FIELD_CONDITION(...)`, `SC_REFLECTION_CLAMPED(min,max)`, `SC_REFLECTION_CLAMPED_EX(...)`, `SC_SERIALIZE_FIELD()` → parsed by `Reflection.py`, output regenerated into `FoundationEngine/ECS/ReflectionRegistry.cpp` (used for editor inspector UI, serialization).
- `SC_PAYLOAD_FIELD(assetType)`, `SC_PAYLOAD_FIELD_EX("name", assetType)` → parsed by `Payload.py`, output regenerated into `FoundationEngine/ECS/PayloadRegistry.cpp` (asset-reference fields).

These macros are no-ops at compile time (defined empty in `FoundationEngine/Prelude.h`) — they exist purely as markers for the codegen scripts. When adding/editing a component or payload field, annotate it with the appropriate macro and let the pre-build step regenerate the registry; do not hand-edit the generated regions (bounded by `// [REFLECTION_AUTO_BEGIN]` / `// [REFLECTION_AUTO_END]` and `// [PAYLOAD_AUTO_BEGIN]` / `// [PAYLOAD_AUTO_END]` markers) — run `Clean.bat` if they get into a bad state.

## Architecture

### Module/project layout and dependency order

`FoundationEngine` is the base dependency for every other module (per `Runtime/Runtime.sln` project dependencies):

```
FoundationEngine
  ├── PhysicsEngine   (wraps JoltPhysics)
  ├── GraphicsEngine  (D3D12 renderer)
  ├── AIEngine
  └── AudioEngine     (wraps CRI ADX2)

Runtime / Editor  depend on: AIEngine, GraphicsEngine, FoundationEngine, AudioEngine, PhysicsEngine
UserProject       depends only on FoundationEngine (builds game/user code independently)
```

Every module includes a single `Prelude.h`/`Prelude.cpp` per project (e.g. `FoundationEngine/Prelude.h`) that centralizes all third-party includes, `#pragma comment(lib, ...)` linking, and shared macros. `FoundationEngine/Prelude.h` is effectively the engine's global precompiled-header-style entry point — most engine `.cpp` files include it (or their own module's Prelude) rather than pulling in individual third-party headers directly.

### External dependencies (`External/`)

Vendored, prebuilt-lib third-party libraries: JoltPhysics (physics), DirectXTK/DirectXTex (D3D12 helpers/textures), DLSS, ImGui (+ ImGuizmo, ImNodeEditor) for editor UI, SDL3 (windowing/input), Cereal (serialization), CRI ADX2 (audio), MTSDF/FreeType/HarfBuzz/msdfgen (text/font rendering), TinyglTF, RecastNavigation (navmesh/pathfinding), WebSocket.

### ECS (`FoundationEngine/ECS`)

Archetype-based ECS (namespace `SeedCore`), similar in shape to Unity DOTS/EnTT:
- `World` is the central API — create/destroy `Entity`/`Actor`, add/remove components. Components route to either archetype-column storage or sparse-set storage per `ComponentMetadata::storage_`.
- `Archetype`/`ArchetypeRegistry`/`Chunk` implement archetype-chunked component storage; `SparseSet` implements the sparse alternative for less-common components.
- `ComponentRegistry`/`ReflectionRegistry`/`PayloadRegistry`/`TagRegistry` are metadata registries — the latter two have code-generated bodies (see Code generation above).
- `Component/` holds built-in components (`Position`, `Rotation`, `Scale`, `Velocity`, lifecycle markers like `Active`, `Startable`, `Tickable`, `FixedTick`, `LateTickable`, `Awakeable`, `Destroyable`, and interaction markers `Collisionable`/`Triggerable`).
- `System/` holds engine systems that operate over queries (`TransformSystem`, `SceneTransitionSystem`), scheduled via `SystemScheduler`.
- `Resource/` handles scenes/prefabs/assets: `Scene`, `Prefab` (+ pools), `ResourceCache`, `LoaderSystem`, `Gateway`, and `ActorSerialization` (Cereal-based).

### Job system (`FoundationEngine/JobSystem`)

A Taskflow-like job graph system (`JobGraph`/`JobNode`/`JobTopology`/`JobExecutor`/`JobWorker`) used for multithreaded scheduling across the engine (also feeds Jolt's `JobSystemWithBarrier`).

### Graphics (`GraphicsEngine`)

DirectX 12 renderer, entry point `Graphics.h`/`Graphics.cpp`. Subfolders: `D3D12/` (device/common D3D12 wrappers — `D3D12Common.h` is included directly from `FoundationEngine/Prelude.h`), `Renderer/`, `Camera/`, `Light/`, `Model/`, `Shader/`, `Shape/`, `Sky/`, `Movie/`, `Font/`, `Raytracing/`, `Resource/`, `Profiler/`, `DLSS/`. Compiled shader binaries (`.cso`) live in the top-level `CompiledShaderObject/` folder, separate from shader source.

### Physics (`PhysicsEngine`)

Thin engine-facing wrapper around JoltPhysics: `Physics/`, `Rigidbody/`, `Softbody/`, `Collider/`.

### AI (`AIEngine`)

`CharacterAI/`, `MetaAI/`, `SpatialAI/` (e.g. navmesh/pathfinding via RecastNavigation).

### Audio (`AudioEngine`)

Wrapper around CRI ADX2 (`CRI/`).

### Runtime vs Editor vs UserProject

- `Runtime/` — the shippable game executable. `Application/Framework.*` + `Application/Main.cpp`.
- `Editor/` — the editor executable/tooling, built on the same engine libraries plus ImGui-based `Panel/` UI, `GizmoContext`, node editor (`NodeEditor.json`).
- `UserProject/` — user/game-specific code and content, decoupled from Runtime/Editor and depending only on `FoundationEngine`. Contains `Assets/`, `Scene/`, `Prefab/`, `SourceCode/`, `DefaultCode/` — these are the folders `Clean.py` clears out. This is where gameplay code and content authored on top of the engine lives.

## Notes

- Build scripts and console/log output in this repo are in Japanese (e.g. `Clean.py` prints `削除:`, `フォルダ削除:`).
- Debug and Release configurations link different lib variants (see the `#ifdef _DEBUG` blocks in `FoundationEngine/Prelude.h`) — don't mix Debug/Release libs across projects.

## Coding conventions

These were derived by reading actual source across `FoundationEngine`/`GraphicsEngine`/`PhysicsEngine`/`Editor`. Follow them for new/edited code; where the codebase itself is inconsistent, that's called out explicitly below.

- **Namespace**: everything lives in `namespace SeedCore { ... }`.
- **No Singletons, in general.** Avoid the Singleton pattern (static instance accessor, `GetInstance()`-style) for new code — prefer passing dependencies explicitly (e.g. through `World`, constructor injection) instead of global/static access. If you think a case genuinely needs one, ask first rather than adding it silently.
- **Types**: never use raw `int`/`float`/`bool`/`std::string` etc. Use the project's own PascalCase aliases from `FoundationEngine/Utility/Types.h`: `Int`/`Int8/16/32/64`, `Uint`/`Uint8/16/32/64`, `Float`, `Double`, `Bool`, `Char`/`Char8/16/32`, `Size`, `Byte`, plus custom containers like `String`, `DynamicArray<T>`.
- **Class/struct/enum/function names**: `PascalCase` (`World`, `ComponentBase`, `TransformSystem`, `AddComponent`, `enum class ComponentStorage { SparseSet, Archetype }`).
- **Member variables**: `camelCase` with a trailing underscore — `handle_`, `actor_`, `componentName_`, `awoken_`, `index_`, `generation_`. This applies to both class and struct private/public data members.
- **Local variables / parameters**: `camelCase`, no underscore (`parentMatrix`, `worldMatrix`, `entity`).
- **Initialization syntax**: for ordinary scalar/object variables, use `=` copy-initialization — `Int i = 1;`, not brace-init `Int i{1};`. Exception: `Desc`-style structs (e.g. `D3D12_RESOURCE_DESC desc{};`) are default/aggregate-initialized with bare `{}`, not `= {}` — write `desc{};`, not `desc = {};`.
- **Getters**: `GetX()` returning by value/reference, usually trailing-`const` (see below), e.g. `GetHandle()`, `GetActor()`, `GetComponent<T>()`. Boolean queries are `HasX()`/`IsX()`/`ExistsX()`-style (`HasComponent`, `Exists`).
- **Header guards**: `#pragma once`, not `#ifndef` guards.
- **Includes**: always full project-relative angle-bracket paths, e.g. `#include <FoundationEngine/ECS/Entity.h>` — never quoted relative includes (`"Entity.h"`).
- **Braces**: Allman style everywhere (opening brace on its own line) — for namespaces, classes, functions, and control flow. Always use braces, even for a single-statement body — no one-liners like `if (x) return;` or `if (x) y = 1;`. Write:
  ```cpp
  if (x)
  {
      return;
  }
  ```
- **Empty function bodies**: write `/// No Code` inside an otherwise-empty `{ }` rather than leaving it blank (used 100+ times across the codebase, e.g. default constructors) — keep doing this rather than an empty block.
- **Doc comments — split by what's being commented, not by comment length:**
  - **`class`/`struct`/`enum class`/function declarations, and their `.cpp` definitions**: always the `/** */` block form, immediately above both the header declaration and the `.cpp` definition, bilingual English then Japanese separated by a divider:
    ```cpp
    /**
    * [EN]
    * <English description>
    *
    * ---------------------------------------------------------------------
    *
    * [JP]
    * <Japanese description>
    */
    ```
  - **Variables** (member variables and local variables): `///`, as a paired `/// [EN] ...` / `/// [JP] ...` bilingual comment above the declaration. Same rule for `enum` values.
  - **Comments inside a function body** (explaining implementation/why): `///`, and also bilingual `/// [EN] ...` / `/// [JP] ...` pairs — same as variables, not a special case.
  Match this bilingual format for new public API surface in `FoundationEngine`-level code; it's less consistently applied in leaf gameplay/renderer code.
  Note: the CLAUDE.md file itself uses plain `//` for its own code-block comments; `///` is the source doc-comment marker, not a formatting hint for this file.
  **Only add these comments when the user explicitly asks for comments** (e.g. "add comments" / "コメント付きで"). Otherwise write code without comments, by default.
  **When comments are requested, err on the side of granularity** — comment liberally rather than sparsely: every non-trivial function/struct/class, every non-obvious member/local variable, and every meaningful step inside a function body should get its `///`/`/** */` pair, not just the parts that seem hardest to follow.
  **Exception — HLSL files that get `#include`d by another shader (typically `.hlsli`): English only, no Japanese.** DXC chokes on Japanese in included files. The top-level `.hlsl` file that does the including (and isn't itself included anywhere) is unaffected — it can still use normal bilingual `[EN]`/`[JP]` comments. So the rule is about being an included file, not about the `.hlsl` vs `.hlsli` extension per se. Where English-only applies, keep the same `/** */`-for-declarations / `///`-for-variables-and-inline structure, just write the English half only.
- **Codegen macros** (`SC_REFLECTION_FIELD()`, `SC_PAYLOAD_FIELD(type)`, etc.) go on the line immediately above the field they annotate, not inline.
- **Component registration**: components register themselves at namespace scope right after the struct, via `REGISTER_COMPONENT(Type, "Category", ComponentStorage::Archetype)` (or `SparseSet`).
- **Indentation: tabs only.** The codebase currently has mixed tabs/4-space indentation (even within single files, e.g. `Actor.h`, `Entity.h`) — that's legacy, not the target style. New/edited code should use tabs; when editing an existing mixed-indent block, convert it to tabs rather than matching the stray spaces.
- **`const` qualifier: no space before it** — write `GetX()const`, not `GetX() const`. The codebase currently has both spacings (~1:2 ratio); no-space is the target style going forward.
- **`for`-loop counters**: no `i`. Use `index`, or a descriptive name ending in `Index` (e.g. `actorIndex`, `chunkIndex`) when there are nested loops or the counter needs disambiguating.
- **File encoding: UTF-8. Line endings: CRLF.** Save all source files this way (matches the Japanese comments/strings throughout the codebase and the Windows/MSBuild toolchain).
- **No D3D12 acronym abbreviations in identifiers** — spell them out in full: `ShaderResourceView` not `Srv`/`SRV`, `UnorderedAccessView` not `Uav`/`UAV`, `ConstantBufferView` not `Cbv`/`CBV`, `RenderTargetView` not `Rtv`/`RTV`, `DepthStencilView` not `Dsv`/`DSV`. This applies to variable/function/type names; using the official D3D12 API types themselves (e.g. `D3D12_SHADER_RESOURCE_VIEW_DESC`) is unaffected. Exception: `Desc` (for "Description") is fine as-is — e.g. `resourceDesc`, `D3D12_RESOURCE_DESC desc{}` — no need to spell out "Description".
- **HLSL/HLSLI variable naming: `snake_case`.** Applies to local variables, function parameters, and global resource/constant-buffer declarations (e.g. `Texture2D splash_texture_ : register(t0);`, `ConstantBuffer<ConstantIndices> constant_indices : register(b0, space1);`). `struct`/`cbuffer` members additionally get a trailing underscore, matching the C++ member convention — e.g. `struct ShadowRayConstantBuffer { float ray_t_max_; float normal_bias_; };` (see `Shadow.hlsli`, `Constants.hlsli` for the established pattern). Function names are unaffected (stay `PascalCase`, e.g. `OctNormalDecode`, `SeedFromPixel`). `#define` macros/constants are unaffected (stay `UPPER_SNAKE_CASE`, e.g. `CLUSTER_TILE_SIZE`).

# Mangifera Architecture Review and Roadmap Design

**Date:** 2026-03-11

**Status:** Approved

**Scope:** Review the current Mangifera repository, identify the architectural gap between the intended render-core design and the current implementation, and define a staged roadmap for stabilizing the Vulkan-first RHI, finishing render-core inversion, expanding rendering features, and preparing the engine for future differentiable rendering workflows.

---

## 1. Executive Summary

Mangifera already contains the first pieces of a Vulkan-first render-core refactor, but the repository is still in a transitional state. The codebase has `Frame_Context`, `Render_Graph`, `Frame_Pipeline`, `Render_Scene`, device capabilities, and focused tests, yet the real execution path still lives largely inside the legacy `Renderer` and `Application` orchestration.

The correct near-term direction is:

1. Stabilize the existing Vulkan-first RHI into a stronger, more explicit, more verifiable interface.
2. Finish the architectural inversion so Render Core becomes the real execution owner.
3. Grow rendering features in the order:
   - `A`: visibility and scale
   - `B`: lighting and image quality
   - `C`: materials and surfaces
   - `D`: data, tooling, sensor outputs, and profiling
4. Make the engine differentiable-ready from phase one without turning the first phase into a full autodiff-native renderer rewrite.

This document recommends a short dual-path transition followed by a single authoritative render path owned by Render Core.

---

## 2. Repository Review

### 2.1 Existing strengths

The repository already contains several valuable assets:

- A workable split across `core`, `graphics`, and `app`.
- A functioning Vulkan backend under `graphics/backends/vulkan`.
- A public RHI seam centered around `graphics::Device`, `Command_Buffer`, `Command_Queue`, resources, sync objects, and pipelines.
- A first pass at device capability discovery in `graphics/capabilities`.
- Existing rendering features including PBR, shadows, IBL, SSR, SSAO, bloom, tonemapping, color grading, and volumetrics.
- A first render-core scaffold under `app/render_core`.
- Focused CTest coverage for smoke, RHI, render-core, headless, and RT-capability plumbing.

These foundations make a clean architectural finish realistic without a full rewrite.

### 2.2 Current architectural blockers

The current codebase is not yet at the approved render-core-centered architecture.

Key blockers:

1. `Render Core` exists, but is still mostly a compatibility layer rather than the engine center.
2. `Renderer` still interprets pass names and drives the real frame orchestration.
3. `Application` still owns too much rendering logic and reaches directly into mutable world state.
4. `Post_Process_Manager` still behaves like a hidden execution center for many post passes.
5. `Render_Scene_Extractor` is still a placeholder.
6. `Render_Graph` is currently only a string-based dependency sorter.
7. RHI semantics are still too weak in public interfaces, with important synchronization and rendering details living only in the Vulkan backend.
8. Material runtime is still effectively a single in-app PBR struct, not an extensible material system.

### 2.3 Current testing reality

The current tests are useful, but most of them validate shape rather than full execution semantics. They are a good foundation for expansion, not yet sufficient evidence for large architectural moves by themselves.

---

## 3. Approved Strategic Direction

The approved direction for Mangifera is:

- `Vulkan-first` today.
- `DX12-ready` later, but not in phase one.
- `Render Core` must become the real execution owner.
- Feature growth must happen on top of Render Core and graph-native passes, not by expanding legacy callback orchestration.
- The first phase must preserve a usable rendering baseline.
- Temporary feature freezes are acceptable only if they are explicitly tracked, reviewed, and paired with a recovery plan.
- Differentiable rendering is a required future capability, but phase one should target `differentiable-ready forward rendering`, not a full autodiff-native engine.

---

## 4. Goals and Non-Goals

### 4.1 Phase-one goals

Phase one must:

- Harden the Vulkan-first RHI into a strong semantic interface.
- Make Render Core the real frame orchestration layer.
- Move the main scene rendering path off legacy callback ownership.
- Introduce real render-scene extraction and render-target/resource ownership.
- Provide the verification and profiling substrate needed for later Vulkan optimization.
- Start feature recovery and expansion in the order `A -> B -> C -> D`.
- Establish differentiable-ready contracts for parameters, replay, and structured outputs.

### 4.2 Explicit non-goals for phase one

Phase one does **not** need to:

- Ship a production DirectX 12 backend.
- Ship a full hybrid ray tracing feature stack.
- Finish a full material taxonomy in one pass.
- Turn Mangifera into a research-first differentiable renderer similar to Mitsuba 3 + Dr.Jit.

---

## 5. Functional Baseline and Completion Standard

### 5.1 Baseline that must remain usable

Each checkpoint in phase one must preserve at least this rendering baseline:

- Application startup
- Scene loading
- Main PBR scene rendering
- Shadow rendering
- IBL
- Current main post-process chain
- Resize handling
- Basic editor/debug UI

### 5.2 Phase-one completion definition

Phase one is complete only when all of the following are true:

- Render Core, not the legacy callback path, owns the main scene execution path.
- `Render_Scene` is a real extracted snapshot.
- Graph-declared passes are guaranteed to execute or fail loudly.
- Resource ownership and render-target lifetime are no longer scattered across `Renderer` and `Post_Process_Manager`.
- Newly added features attach as graph-native passes.
- The system includes enough validation, profiling, and observability to support the next Vulkan performance phase.
- The forward renderer is differentiable-ready via stable scene snapshots, stable output products, replay contracts, and parameter registration boundaries.

---

## 6. Target Layer Responsibilities

### 6.1 `Application`

`Application` should become a shell and editor/runtime coordinator, not the rendering brain.

Responsibilities:

- Input, UI, world update, editor/runtime control
- Triggering render extraction
- Building frame requests
- Handing the request to Render Core

It should no longer directly determine main render sequencing or pull render-time state from the world during pass execution.

### 6.2 `Renderer`

`Renderer` should become a device/presentation shell and backend execution service.

Responsibilities:

- Device and swapchain lifetime
- Begin/end frame and presentation
- Low-level command recording support
- GPU annotations, timestamps, capture hooks

It should no longer own the semantic meaning of passes such as `"scene_render"` or `"sensor_export"`.

### 6.3 `Render Core`

Render Core must become the engine center.

Required production objects:

- `Frame_Context`
- `Render_Scene`
- `Render_Product_Request`
- `Render_Graph`
- `Frame_Pipeline`
- `Frame_Executor`
- `Render_Resource_Registry`

Responsibilities:

- Determine what happens this frame
- Determine what outputs are required
- Build pass/resource dependencies
- Compile and execute the frame plan
- Own the resource plan and resource lifetime model

### 6.4 `Post Process`

`Post_Process_Manager` must stop acting as a hidden execution center.

End-state role:

- Pass library
- Optional short-term compatibility facade

It should expose passes, not privately own the authoritative post pipeline schedule.

### 6.5 `RHI`

The RHI must become a stronger semantic interface with Vulkan-first fidelity.

Principles:

- Do not prematurely flatten Vulkan into a weak least-common-denominator API.
- Do not keep backend-critical semantics trapped inside the Vulkan implementation.
- Keep the abstraction explicit and validation-friendly.

---

## 7. RHI Design Requirements

Phase one RHI work must add or harden the following concepts:

### 7.1 Resource semantics

- Stronger buffer and texture usage flags
- Binding intent
- Memory domain
- Export/readback intent
- Transient/persistent ownership intent
- Debug names

### 7.2 Synchronization semantics

- Stronger resource states
- Barriers with subresource ranges
- Queue ownership transfer
- Stage and access semantics that are visible at the public abstraction layer

### 7.3 Rendering abstraction

- Attachment-based rendering info
- Reduced upper-layer dependence on Vulkan render-pass/subpass semantics
- Enough structure to support Vulkan dynamic rendering and a future DX12 backend

### 7.4 Command submission semantics

- Explicit queue/list type
- Explicit allocator/pool intent
- Debug annotation and timestamp hooks

### 7.5 Capability matrix

The capability model must become rich enough to gate real features, not just a few booleans.

At minimum:

- Queue topology
- Dynamic rendering
- Descriptor indexing
- Timeline synchronization
- Ray tracing support
- Relevant format/sample-count support
- Memory budget visibility

### 7.6 RT-ready placeholders

Phase one should define the interface boundaries for:

- Acceleration structures
- RT descriptor binding
- Trace/build command boundaries
- Shader-table or equivalent RT-dispatch contract

This does not require shipping final RT features yet.

---

## 8. Render Core Design Requirements

### 8.1 `Frame_Context`

`Frame_Context` must evolve beyond width/height/mode.

It should include:

- Monotonic frame id
- Resolution and view configuration
- Run mode
- Quality tier
- Debug toggles
- Requested render products
- Timing data needed for temporal systems

### 8.2 `Render_Scene`

`Render_Scene` must be a stable extracted snapshot, not an empty shell.

It should contain:

- Mesh instances
- Cameras/views
- Directional/point/spot lights
- Per-instance transforms
- Material bindings
- Stable instance/object identifiers
- Previous-frame data needed for motion and temporal effects
- Optional semantic metadata for sensors and future differentiable workflows

### 8.3 `Render_Product_Request`

The frame must declare requested products explicitly. These should eventually include:

- RGB
- Depth
- Normal
- Motion vector
- Instance id
- Semantic id
- Material feature buffers where needed

### 8.4 `Render_Graph`

The graph must evolve from string sorting to a minimal executable graph system.

At minimum, passes and resources must carry:

- Pass identity
- Logical resources
- Read/write usage
- Queue affinity
- Imported vs transient vs persistent ownership
- Execution order
- State-transition plan

### 8.5 `Frame_Executor`

The executor must take the compiled plan and record the real work. This is the bridge that makes the graph authoritative instead of decorative.

### 8.6 `Render_Resource_Registry`

The resource system must define:

- Imported resources
- Persistent resources
- Transient resources
- Resize-aware outputs
- Future readback/export resources

---

## 9. Feature Roadmap Inside the New Architecture

Rendering features should be integrated in the approved order.

### 9.1 `A`: Visibility and scale

First expansion priority:

- Real depth prepass
- Frustum culling
- Hi-Z resource and pass
- Interfaces for later occlusion culling and indirect drawing

This area should shape the graph and resource design early.

### 9.2 `B`: Lighting and image quality

Second expansion priority:

- Improved shadow structure
- Stable graph-native SSR/SSAO
- Volumetric effects
- Explicit RT bridge points for future reflections/shadows

### 9.3 `C`: Materials and surfaces

Third expansion priority:

- Textured PBR opaque
- Normal mapping
- Fixed emissive behavior
- Masked materials
- Transparent material contract
- Later extension points for clearcoat, transmission, anisotropy, and beyond

### 9.4 `D`: Data, tooling, and sensor outputs

Fourth expansion priority:

- Sensor outputs
- Headless/shared-kernel maturity
- Profiling overlays
- Validation views
- Replay/export infrastructure

---

## 10. Material Runtime Direction

The current material path is too narrow and too coupled to app-side logic.

The recommended runtime structure is:

- `Material_Definition`
- `Material_Instance`
- `Material_Runtime`
- `Material_Parameter_Block`

Phase-one material goals:

- Replace the current single-struct mentality with a scalable runtime contract
- Support textured PBR as the first serious material runtime
- Ensure material parameters become structured, observable, and future-optimizable

This is the minimum needed to support later surface-model growth and differentiable parameter tracking.

---

## 11. Differentiable-Ready Design Direction

Mangifera should become differentiable-ready during phase one without becoming an autodiff-native renderer yet.

### 11.1 What differentiable-ready means here

The system should support:

- Stable extracted scenes
- Replayable frame packages
- Explicit requested products
- Stable parameter boundaries
- Deterministic forward execution as far as practical

### 11.2 Required contracts

#### `Differentiable_Parameter_Registry`

Future-optimizable parameter categories should have a stable registration boundary:

- Camera pose
- Light parameters
- Material scalars
- Texture references
- Geometry transforms
- Later mesh, texture, and neural-field data

#### `Replayable_Frame_Package`

A frame package should preserve everything needed to replay a forward render consistently:

- Scene snapshot
- Product request
- Camera/light/material state
- Relevant feature toggles

#### `Differentiable-friendly products`

The engine should preserve not only final RGB, but also structured forward outputs such as depth, normals, motion, ids, and material-related buffers.

### 11.3 What phase one should not do

Phase one should **not** attempt to turn the whole engine into a Mitsuba-like autodiff-native renderer. That would dominate the phase and block the more urgent architecture finish.

### 11.4 Longer-term differentiable execution direction

After phase one, the most practical route is:

- Keep Mangifera as a strong real-time forward core
- Add a separate differentiable execution path or backend
- Reuse the stable scene/material/product/replay contracts from phase one

This is more practical for inverse optimization, reconstruction, and neural training than trying to force full autodiff into the initial refactor.

---

## 12. Review, Freeze, and Verification Policy

### 12.1 Feature freezes

Temporary feature freezes are allowed only if each freeze records:

- Feature name
- Freeze reason
- Affected scope
- Regression severity
- Planned recovery checkpoint

### 12.2 Required checkpoint review

Every checkpoint must produce a structured review containing:

- Architectural changes
- Preserved functionality
- Frozen or degraded functionality
- Validation evidence
- Performance evidence
- Current risks
- Next-step prerequisites

### 12.3 Required evidence

Each checkpoint must preserve or produce:

- Tests
- Visual baselines
- Execution evidence
- Performance evidence
- Known-issue list

This requirement exists to enforce the approved constraint that functionality remains usable while architecture changes.

---

## 13. Recommended Checkpoints

The recommended checkpoint order is:

1. `CP0`: Establish baseline scenes, output checks, profiling checks, and review template
2. `CP1`: Harden RHI semantics
3. `CP2`: Land real render-scene extraction and resource registry
4. `CP3`: Make Render Graph + executor authoritative and move the main path off legacy callbacks
5. `CP4`: Add `A` features for visibility and scale
6. `CP5`: Add `B` features for lighting and image quality
7. `CP6`: Add `C` features for material/runtime growth
8. `CP7`: Add `D` features for data tooling, sensors, and differentiable-ready replay/parameter systems

---

## 14. Technologies and External Reference Projects

### 14.1 Mangifera stack

- C++20
- CMake
- Vulkan
- GLFW
- shaderc
- spirv-reflect
- GLM
- CTest

### 14.2 Reference projects informing this design

- `Granite`  
  Vulkan-first render graph and resource/lifetime orchestration  
  https://github.com/Themaister/Granite

- `NVRHI`  
  Practical explicit abstraction with validation and RT-capable resource/state handling  
  https://github.com/NVIDIA-RTX/NVRHI

- `NRI`  
  Explicit low-level interface design across Vulkan and D3D12  
  https://github.com/NVIDIA-RTX/NRI

- `Falcor`  
  Render graph plus modular pass organization for advanced rendering workflows  
  https://github.com/NVIDIAGameWorks/Falcor

- `Filament`  
  Strong material/runtime design direction  
  https://github.com/google/filament

- `nvdiffrast`  
  Modular differentiable raster primitives  
  https://github.com/NVlabs/nvdiffrast

- `Mitsuba 3` and `Dr.Jit`  
  Useful differentiable-rendering reference, but too large a conceptual leap for Mangifera phase one  
  https://github.com/mitsuba-renderer/mitsuba3  
  https://github.com/mitsuba-renderer/drjit

- `redner`  
  Inverse rendering and reconstruction-oriented differentiable rendering reference  
  https://github.com/BachiLi/redner

---

## 15. Final Recommendation

Proceed with a `Vulkan-first, Render-Core-centered` architectural finish.

Do **not** keep scaling the current mixed system where render-core types exist but legacy orchestration still owns the real frame.

Adopt the following near-term rule set:

- Harden RHI before broadening backend ambitions
- Finish render-core inversion before broadening feature ambitions
- Grow rendering features only through graph-native passes
- Preserve a real functional baseline at every checkpoint
- Make the forward renderer differentiable-ready from the beginning of phase one

With this direction, Mangifera can evolve into:

- A stable real-time Vulkan renderer
- A later multi-backend engine candidate
- A future RT-capable renderer
- A materially richer rendering system
- A useful forward core for inverse rendering, reconstruction, and neural-scene workflows


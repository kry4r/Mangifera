# Mangifera Rendering Engine Architecture Design

**Date:** 2026-03-07

**Status:** Approved

**Scope:** Review the current repository, identify architectural risks, and define a complete target architecture for a high-fidelity, high-performance rendering engine that remains suitable for future physics simulation and embodied-AI training.

---

## 1. Vision

Mangifera should evolve from a feature-rich Vulkan rendering demo into a unified rendering platform with the following product goals:

- Deliver high-quality real-time rendering for interactive applications.
- Support editor, runtime, and headless execution modes on a shared engine core.
- Preserve a Vulkan-first RHI while preparing for future multi-backend support.
- Treat training-friendly sensor outputs as first-class render products.
- Remain extensible for future physics simulation, embodied-intelligence workflows, and hybrid ray tracing.

This is not a path-tracing-first renderer. It is a raster-first real-time renderer with a clean path toward hybrid RT and large-scale headless workloads.

---

## 2. Approved Product Direction

The approved design assumptions are:

- **Primary engine type:** High-fidelity mixed rendering platform.
- **Primary runtime target:** Real-time interaction with high visual quality.
- **Secondary runtime target:** Large-scale headless simulation and training throughput.
- **Platform strategy:** Multi-platform, multi-backend architecture in principle; Vulkan-first in implementation.
- **Domain strategy:** General-purpose simulation platform, not indoor-only or outdoor-only.
- **Execution model:** Unified kernel for editor, runtime, and headless modes.
- **Architectural center:** Renderer-centric engine, with simulation systems attached around it.
- **Required outputs:** RGB, depth, normal, segmentation, instance ID, and motion vector.
- **Lighting strategy:** Raster-first today, explicit architectural preparation for hybrid RT.
- **Refactor appetite:** Large foundational refactor is acceptable.

---

## 3. Repository Review

### 3.1 Existing strengths

The repository already contains several valuable assets:

- A recognizable three-layer split across `core`, `graphics`, and `app`.
- A functioning Vulkan backend under `graphics/backends/vulkan`.
- A usable RHI skeleton with `Device`, `Command_Buffer`, `Buffer`, `Texture`, `Pipeline`, `Render_Pass`, and sync abstractions.
- A growing collection of rendering features including PBR, shadows, IBL, and multiple post-process effects.
- A world/component foundation in `core/manager/world.hpp` and scene representation in `core/manager/scene-graph.hpp`.

These pieces make a full rewrite unnecessary.

### 3.2 Current structural problems

The repository is currently closer to a feature-accumulated renderer demo than to a stable engine core.

Key issues:

1. **Renderer orchestration is too centralized.**
   - `app/renderer/renderer.hpp` and `app/renderer/renderer.cpp` mix device setup, swapchain control, render target management, synchronization, and frame orchestration.

2. **Frame execution is callback-driven rather than graph-driven.**
   - `set_render_callback`, `set_post_process_callback`, and related hooks indicate that pass dependency management lives outside a formal frame orchestration system.

3. **Post-processing is grouped, but not yet systematized.**
   - `app/post_process/post_process_manager.hpp` centralizes many effects, but it is still a manager, not a pass graph or a composable feature pipeline.

4. **World and scene systems are not yet ready for simulation-grade extraction.**
   - `World` acts as a flexible ECS-like component store, but not yet as a scheduling and snapshot system suitable for rendering, physics, and training data extraction.

5. **RHI abstractions are correct in shape, but not yet engine-grade in semantics.**
   - Current RHI interfaces expose creation and recording primitives, but lack a formal capability model, resource intent semantics, and graph-oriented execution support.

---

## 4. Chosen Strategy

Three broad migration strategies were considered:

- Gradual cleanup around the existing renderer.
- Medium-scale refactor around a renderer-centered engine core.
- Full new-engine rewrite.

The chosen strategy is:

> **Keep the current RHI direction and Vulkan investment, but rebuild the engine center around Render Core, Frame Graph, resource management, and unified runtime modes.**

This approach maximizes reuse while still fixing the architectural bottlenecks that would otherwise block future growth.

---

## 5. Target Layered Architecture

The target architecture should be organized into six logical layers.

### 5.1 Platform Layer

Responsibilities:

- Windowing
- Input
- File system
- Timing
- Configuration
- Logging
- Threading primitives

This layer contains mode-specific shell behavior for editor, runtime, and headless applications, but does not own rendering logic.

### 5.2 RHI Layer

Responsibilities:

- Device abstraction
- Resource creation
- Command recording
- Synchronization
- Queue management
- Capability queries

Design rules:

- Preserve current `graphics` layer concepts.
- Keep Vulkan as the first implementation.
- Avoid reducing the API to the lowest common denominator too early.
- Add explicit capability and limit queries to support future D3D12 and Metal backends.

### 5.3 Render Core Layer

This is the new engine center.

Core objects:

- `FrameContext`
- `RenderScene`
- `RenderGraph`
- `PassRegistry`
- `RenderResourceCache`
- `TransientResourcePool`
- `FramePipeline`

This layer decides what happens each frame, what resources exist, how passes depend on each other, and which outputs are produced for each execution mode.

### 5.4 Renderer Feature Layer

This layer contains composable rendering features and passes:

- Geometry / depth prepass
- G-buffer or forward+ material passes
- Shadows
- Lighting
- Sky and atmosphere
- SSAO
- SSR
- Volumetrics
- Bloom
- Tonemapping
- Sensor outputs
- Hybrid RT passes in future

### 5.5 Simulation / World Layer

This layer owns:

- Entities and components
- Scene graph
- Physics state
- Sensor attachment metadata
- Timeline and state extraction

The renderer consumes a curated render snapshot from this layer rather than reaching directly into mutable world state.

### 5.6 Tools Layer

This layer includes:

- Editor shell
- Asset pipeline tools
- Headless batch runner
- Replay tooling
- Profiling and debugging tools

---

## 6. Render Graph and Frame Pipeline

### 6.1 Why a graph is required

The renderer has already reached the point where manual orchestration is a liability. Frame setup should be expressed as a graph of passes and resources rather than as scattered control logic inside `Renderer` and `Post_Process_Manager`.

### 6.2 Required render core objects

#### FrameContext

Must include:

- Frame index
- Delta time
- Viewport and resolution
- Run mode: editor, runtime, headless
- Quality tier
- Debug toggles
- Requested sensor outputs

#### RenderScene

Must be an extracted snapshot containing:

- Visible mesh instances
- Materials and material bindings
- Cameras and per-view state
- Lights
- Previous-frame history for motion vectors and temporal effects
- Stable object and instance identifiers

#### RenderGraph

Each pass must declare:

- Read resources
- Write resources
- Queue type
- Dependency edges
- Transient or persistent resource requirements

The graph should derive:

- Execution order
- Resource lifetime
- State transitions
- Potential aliasing opportunities
- Future async-compute placement

### 6.3 Recommended first frame pipeline

1. Visibility and culling
2. Depth prepass
3. Motion vector generation
4. Geometry / G-buffer or forward material pass
5. Shadow passes
6. Direct lighting and IBL
7. Screen-space effects
8. Atmosphere and volumetrics
9. Post-processing
10. Sensor output export
11. Presentation or headless readback

### 6.4 Sensor outputs as first-class products

The following outputs must be formal render products from phase one:

- RGB
- Depth
- Normal
- Segmentation
- Instance ID
- Motion Vector

Segmentation should support both semantic class IDs and instance IDs.

These outputs should not be treated as ad hoc debug captures. They should be formally scheduled and named within the frame graph.

---

## 7. Resource, Material, and Shader Systems

### 7.1 Render resource cache

The engine needs a layer between logical assets and physical GPU objects.

Required sub-systems:

- Persistent GPU resource cache
- Transient frame resource pool
- Resize-aware render target registry
- Readback/export resources for headless mode

This enables multiple viewports, quality tiers, sensor outputs, and headless export without uncontrolled resource churn.

### 7.2 Material system

The engine should adopt a three-level material model:

- `MaterialDefinition`
- `MaterialInstance`
- `MaterialRuntime`

This should support at least:

- Opaque
- Masked
- Transparent
- Emissive
- Sensor-only/debug materials

The material system must feed the shader variant and PSO caching layers instead of letting permutations sprawl uncontrolled.

### 7.3 Shader and pipeline variant system

The engine needs formal control over shader permutations.

Required concepts:

- `ShaderSource`
- `ShaderVariantKey`
- `PipelineKey`
- Reflection cache
- PSO cache

Initial variant dimensions should include:

- Static vs skinned
- Alpha mode
- Velocity output enabled/disabled
- Sensor pass enabled/disabled
- Ray tracing available/disabled
- Quality tier differences

### 7.4 Asset pipeline

The engine should move toward cooked internal formats for:

- Meshes
- Materials
- Textures
- Scene prefabs
- Lighting probes and caches

Future metadata for simulation should include:

- Semantic labels
- Collision metadata
- Articulation and joint metadata
- Sensor mount metadata

---

## 8. RHI Review and Refactor Direction

### 8.1 What should be preserved

The current RHI shape is good and should be preserved in principle:

- `Device`
- `Buffer`
- `Texture`
- `Sampler`
- `Shader`
- `Descriptor_Set`
- `Pipeline_State`
- `Command_Buffer`
- `Command_Queue`
- Sync objects

This is a sound base for a Vulkan-first engine.

### 8.2 What must change

The RHI should be upgraded from a thin backend abstraction to an engine-grade execution interface.

Required additions:

- Formal device capability and limit reporting
- Format support queries
- Queue topology and feature exposure
- Resource intent semantics
- Better naming/debug metadata support
- A bridge from logical pass graph scheduling to backend command recording

### 8.3 Specific RHI refactor goals

#### Device capabilities

Add a structured device capability model containing:

- Ray tracing support
- Descriptor indexing support
- Dynamic rendering support
- Timeline semaphore support
- Async compute availability
- Supported formats and sample counts
- Memory budget visibility

#### Resource descriptors

`Buffer_Desc` and `Texture_Desc` should gain semantics beyond creation shape:

- Usage flags
- Bind flags
- Memory domain
- CPU mapping policy
- Transient / aliasable status
- Export/readback intent
- Debug name

#### Descriptor and binding model

Keep descriptor sets, but define a higher-level binding model:

- Frame-global bindings
- Scene-global bindings
- Material bindings
- Pass-local bindings

#### Render pass abstraction

Allow Vulkan to use render passes, subpasses, or dynamic rendering internally, but move upper-layer orchestration away from hard dependence on `Render_Pass` as the primary planning abstraction.

### 8.4 RHI validation track

RHI refactoring should include a dedicated validation track:

- Capability matrix documentation
- Resource state validation
- Descriptor completeness checks
- Backend conformance smoke tests
- Golden-path render tests for Vulkan

This should be treated as a first-class subproject during the engine refactor.

---

## 9. Unified Kernel for Editor, Runtime, and Headless

The engine should run a shared kernel with different shell frontends.

### 9.1 Shared runtime loop

The target per-frame order is:

1. Input and commands
2. World update
3. Physics update
4. Sensor synchronization
5. Render extraction
6. Frame graph execution
7. Output, record, or presentation

### 9.2 Mode shells

- `EditorShell`: tooling UI, multiple viewports, inspection, hot reload.
- `RuntimeShell`: game or application mode.
- `HeadlessShell`: no presentation, batch jobs, dataset generation, replay.

These shells should only differ in bootstrap behavior and enabled features, not in the underlying renderer and simulation kernel.

---

## 10. Physics and Embodied-AI Readiness

Although the engine remains renderer-centric, it must be designed to host future simulation systems.

Required integration points:

- Stable world timeline
- System scheduling groups
- Physics state extraction into render snapshots
- Sensor attachment and mount definitions
- Deterministic replay support
- Dataset export contracts

The renderer should not directly own physics, but it must expose stable interfaces for sensor generation and replayable outputs.

---

## 11. Performance Strategy

Performance must be considered across both real-time and headless workloads.

### 11.1 Performance goals

- Low-latency interactive rendering for editor/runtime
- Stable frame pacing
- High-throughput sensor generation in headless mode
- Controlled memory use through transient resource reuse

### 11.2 Required pillars

- Frustum and instance culling
- Future Hi-Z occlusion support
- Transient render target aliasing
- Queue-aware uploads and transfers
- Quality tiers for interactive, cinematic, dataset, and training modes
- Sensor-selective rendering for throughput-oriented runs

### 11.3 Hybrid RT rollout

Architectural support should exist from the beginning, but feature rollout should remain staged:

1. Raster-first base renderer
2. Optional RT shadows or reflections
3. Temporal accumulation and denoising
4. Later RT GI where justified

---

## 12. Validation and Testing Strategy

The engine needs formal validation to remain maintainable through large refactors.

### 12.1 Test layers

- RHI unit tests
- Render graph dependency tests
- Golden image tests
- Headless dataset export tests
- Sensor contract tests

### 12.2 Debugging infrastructure

Required tooling:

- GPU debug labels
- Frame capture integration hooks
- Render validation checks
- Deterministic replay and frame reproduction

This infrastructure is essential for future embodied-AI data integrity and rendering correctness.

---

## 13. Recommended Target Module Layout

Recommended future structure:

- `engine/platform`
- `engine/rhi`
- `engine/render-core`
- `engine/render-features`
- `engine/scene`
- `engine/physics`
- `engine/sensors`
- `engine/assets`
- `tools/editor`
- `tools/headless`
- `apps/runtime`

Migration mapping:

- `graphics` -> `engine/rhi`
- `core` -> `engine/scene` and shared foundation pieces
- `app/renderer` and `app/post_process` -> `engine/render-core` and `engine/render-features`
- `app/application` and `app/window` -> shell applications

---

## 14. Phased Refactor Roadmap

### Phase 0: Stabilize and inspect RHI

- Document current RHI capabilities and limitations.
- Add a capability model and semantic resource descriptors.
- Establish baseline scenes and golden outputs.
- Build minimal RHI validation coverage.

### Phase 1: Extract Render Core

- Separate frame context, render extraction, and pass definitions from `Renderer`.
- Turn sensor outputs into formal render products.
- Replace callback-based flow with explicit pass sequencing.

### Phase 2: Introduce Render Graph and resource management

- Add `RenderGraph`, `TransientResourcePool`, and `RenderResourceCache`.
- Move barrier and lifetime handling into graph execution.
- Define multiple frame pipelines for interactive and headless modes.

### Phase 3: Material, shader, and asset systems

- Build material definitions, runtime bindings, variant control, and PSO cache.
- Introduce cooked asset formats and simulation metadata.

### Phase 4: Unified shells and headless workflows

- Build shared kernel entry points.
- Add headless batch and replay workflows.
- Align editor/runtime/headless on one execution model.

### Phase 5: Hybrid RT and advanced optimization

- Add acceleration structure management.
- Introduce optional RT passes.
- Expand async compute, culling, streaming, and scalability systems.

---

## 15. Success Criteria

The redesign is successful when the engine can do the following:

- Run editor, runtime, and headless modes on the same render core.
- Produce RGB, depth, normal, segmentation, instance ID, and motion vector outputs as formal products.
- Add or remove rendering features without modifying a single monolithic renderer controller.
- Reuse RHI investment while exposing backend capabilities cleanly.
- Host future physics and embodied-AI systems without re-centering the whole engine architecture.

---

## 16. Final Recommendation

Proceed with a medium-scale renderer-centered refactor that preserves the current RHI direction but rebuilds the engine center around Render Core, Render Graph, resource management, and unified runtime modes.

Do not continue scaling the current callback-driven renderer architecture. That path will slow down rendering evolution, tool development, and simulation integration at the same time.

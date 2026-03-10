# Mangifera

Mangifera is a Vulkan-first rendering engine refactor that is moving from a monolithic renderer toward a renderer-centered core with explicit frame orchestration, graph-friendly feature passes, and headless simulation entry points.

## Architecture Entry Points

- `graphics/` holds the Vulkan-first RHI, device capabilities, and resource semantics.
- `app/render_core/` holds `Frame_Context`, `Render_Scene`, `Render_Graph`, and `Frame_Pipeline`.
- `app/render_features/passes/` holds graph-friendly feature pass registration units.
- `app/headless/` holds the headless bootstrap path for batch-style execution.

## Build

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

## Tests

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Useful focused targets:

```powershell
cmake --build build --config Debug --target mangifera_render_core_tests mangifera_headless_tests mangifera_raytracing_capability_tests
ctest --test-dir build -C Debug --output-on-failure -R "frame_context|render_scene_extractor|render_graph|render_targets|sensor_pass|headless_runner|raytracing_capability"
```

## Headless Mode

Run the bootstrap headless path with:

```powershell
build\Debug\Mangifera.exe --headless --frames 2
```

This currently exercises the shared bootstrap path and logging flow while the full no-window batch renderer is being built out.

# Prompts for AI-Assisted Development

Use these prompts one phase at a time. Ask the AI to inspect the existing repository before making changes, preserve existing work, and provide tests for every non-trivial feature.

## Global instructions

```text
You are helping develop a cross-platform C++20 engine for macOS and Linux.
The engine supports both 2D and 3D rendering and is intended for a room-design application.
Use CMake, SDL3, bgfx, GLM, Dear ImGui, and glTF/GLB assets where appropriate.

Before changing code:
1. Inspect the repository and summarize relevant existing files.
2. Explain the proposed design briefly.
3. Make the smallest coherent change.
4. Add or update tests.
5. Build and run the relevant tests.
6. Report changed files, verification results, and remaining risks.

Keep room-design domain logic independent from rendering implementation.
Use meters internally, stable IDs, versioned project files, and clear ownership.
Do not introduce a new dependency without explaining why it is needed.
```

## Phase 0 — Product definition

```text
Help define the MVP for a 2D/3D room-design application.
Describe the primary user workflows, the minimum feature set, non-goals,
acceptance criteria, and the first vertical slice from room creation to saved project.
Keep the scope suitable for a small engineering team.
```

## Phase 1 — CMake and platform foundation

```text
Set up the C++20 CMake foundation for a macOS and Linux application.
Add SDL3, Debug/Release configurations, compiler warnings, formatting,
tests, and a minimal executable. Keep dependencies reproducible and explain
how to build on both platforms. Do not add rendering yet.
```

## Phase 2 — Engine core

```text
Implement the engine core for entities, transforms, resources, input actions,
logging, timing, and serialization interfaces. Use simple, testable C++ types.
Add unit tests for transforms and stable entity IDs. Avoid premature ECS complexity.
```

## Phase 3 — Renderer

```text
Implement a renderer abstraction suitable for both 2D and 3D.
Create the application frame lifecycle, camera types, buffers, textures,
materials, shader management, and debug drawing. Use bgfx while keeping the
rest of the engine independent from bgfx-specific types. Render a triangle,
grid, and axes on macOS and Linux.
```

## Phase 4 — 2D engine

```text
Implement the 2D renderer and viewport.
Support orthographic camera controls, sprites, textured quads, basic shapes,
layers, hit testing, and selection. Add a small demo scene and tests for
screen-to-world and world-to-screen coordinate conversion.
```

## Phase 5 — 3D engine

```text
Implement the 3D renderer.
Support meshes, depth testing, materials, a perspective camera, lighting,
raycasting, and GLB/glTF loading. Add a sample room scene and test selection
coordinates and asset loading failures.
```

## Phase 6 — Room domain model

```text
Design and implement the room-design data model.
Include rooms, wall segments, floors, ceilings, doors, windows, furniture,
materials, stable IDs, validation, and versioned serialization. Keep the model
free of renderer types. Add tests for invalid walls, openings, duplicate IDs,
and save/load round trips.
```

## Phase 7 — Floor-plan editor

```text
Implement a 2D floor-plan editor for the room model.
Support wall drawing, endpoint editing, grid snapping, corner snapping,
numeric dimensions, selection, deletion, door/window placement, and room
validation. Add command objects so every edit can later support undo and redo.
```

## Phase 8 — Procedural 3D room generation

```text
Generate 3D wall, floor, ceiling, door, and window geometry from the room model.
Handle wall thickness, openings, normals, UVs, and material assignment.
Update the 3D scene whenever the validated 2D model changes. Add geometry tests
for wall dimensions and opening placement.
```

## Phase 9 — Furniture catalog

```text
Implement a small furniture catalog using GLB/glTF assets.
Define asset metadata, thumbnails, categories, default scale, bounding boxes,
placement, rotation, wall alignment, and overlap warnings. Add caching and
clear errors for missing or invalid assets.
```

## Phase 10 — Editing workflow

```text
Implement command-based undo/redo, copy/paste, duplication, multi-selection,
properties editing, object locking, visibility, keyboard shortcuts, autosave,
and project open/save. Test that every command can execute, undo, redo, and
serialize safely.
```

## Phase 11 — Export and presentation

```text
Add useful presentation features without changing the room model.
Implement improved lighting, material controls, screenshot export, project
JSON export/import, basic GLB export, and floor-plan image export. Document
which metadata cannot be represented in GLB.
```

## Phase 12 — Performance and release

```text
Profile the engine with large rooms and 100–300 furniture objects.
Measure frame time, loading time, CPU usage, GPU memory, and project size.
Apply only evidence-based optimizations such as instancing, caching, lazy
loading, texture compression, and level of detail. Add release checklists for
macOS and Linux and document known limitations.
```

## Code review prompt

```text
Review this change as a senior C++ engine developer.
Look for lifetime bugs, undefined behavior, incorrect coordinate systems,
platform-specific assumptions, unnecessary coupling between domain and renderer,
serialization compatibility issues, performance problems, and missing tests.
Return findings ordered by severity with file and line references, then suggest
the smallest safe fixes.
```

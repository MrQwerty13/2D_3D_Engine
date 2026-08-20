# 2D/3D Room Design Engine Roadmap

## Product direction

Build a cross-platform C++ engine for macOS and Linux that supports both 2D editing and 3D visualization. The room-design application should use the 2D floor plan as the source of truth and generate the corresponding 3D scene from it.

## Development principles

- Build one shared engine for 2D and 3D, not two unrelated engines.
- Keep room-design rules independent from rendering code.
- Use meters internally and convert units only in the user interface.
- Prefer simple, testable systems before advanced rendering features.
- Keep project files versioned and migratable from the beginning.
- Validate each phase with a working vertical slice.

## Phase 0 — Product definition

Define the first release:

- Create and edit rooms.
- Add doors and windows.
- Add furniture from a small catalog.
- Edit in 2D and inspect in 3D.
- Apply basic materials.
- Save, load, undo, redo, and export screenshots.

Deliverable: a short product specification, interaction sketches, and acceptance criteria for the MVP.

## Phase 1 — Build and platform foundation

Implement:

- CMake project structure.
- C++20 compiler configuration.
- SDL3 window, input, and application lifecycle.
- Logging, assertions, timing, and error handling.
- Debug and release builds on macOS and Linux.
- Unit-test framework and continuous integration.

Deliverable: an empty application window that builds on both platforms.

## Phase 2 — Engine core

Implement shared systems:

- Vectors, matrices, quaternions, and transforms.
- Entity and component or scene-object model.
- Resource handles and asset cache.
- Scene hierarchy.
- Input actions.
- File paths and project directories.
- Basic serialization interfaces.

Deliverable: entities with transforms can be created, updated, and serialized without a renderer.

## Phase 3 — Rendering foundation

Implement the renderer abstraction and first backend:

- Renderer interface.
- Metal backend path for macOS.
- Vulkan backend path for Linux.
- Swapchain and frame lifecycle.
- Vertex/index buffers and textures.
- Shader and material abstractions.
- Orthographic and perspective cameras.
- Debug grid, axes, and frame statistics.

Deliverable: the same sample scene renders on macOS and Linux.

## Phase 4 — 2D engine

Implement:

- Sprites and texture atlases.
- Lines, rectangles, circles, and polygons.
- Text rendering.
- Orthographic camera controls.
- Layers and draw order.
- Tilemap support.
- 2D hit testing and selection.

Deliverable: a small 2D editor can draw, select, move, and save objects.

## Phase 5 — 3D engine

Implement:

- Mesh rendering.
- Materials and PBR textures.
- Directional, point, and ambient lighting.
- Depth testing and face culling.
- Model loading from GLB/glTF.
- Orbit, pan, and first-person camera controls.
- Raycasting and 3D object selection.

Deliverable: a furnished sample room can be loaded and inspected in 3D.

## Phase 6 — Room-design domain

Create the application model:

- Room and room boundaries.
- Wall segments with length, height, and thickness.
- Floors and ceilings.
- Doors and windows attached to walls.
- Furniture instances.
- Materials and design metadata.
- Room validation rules.

Deliverable: a room can be represented completely as a versioned project document.

## Phase 7 — 2D floor-plan editor

Implement:

- Draw and edit wall segments.
- Grid, endpoint, corner, and wall snapping.
- Numeric length and angle editing.
- Room polygon validation.
- Door and window placement.
- Selection, duplication, and deletion.
- Keyboard shortcuts.

Deliverable: the user can create a valid room without editing raw data.

## Phase 8 — Procedural 3D room generation

Generate 3D geometry from the room model:

- Wall meshes from 2D segments.
- Floor and ceiling meshes.
- Door and window openings.
- Correct normals and UV coordinates.
- Interior and exterior materials.
- Immediate synchronization from 2D edits to 3D.

Deliverable: every valid 2D room change updates the 3D room reliably.

## Phase 9 — Furniture and catalog

Implement:

- Asset metadata and categories.
- Preview thumbnails.
- GLB/glTF loading.
- Drag-and-drop placement.
- Rotation, scaling, and wall alignment.
- Bounding boxes and overlap warnings.
- Asset caching and lazy loading.

Deliverable: users can furnish a room with a small, optimized catalog.

## Phase 10 — Editor workflow

Implement:

- Command-based undo and redo.
- Copy and paste.
- Multi-selection.
- Properties inspector.
- Object locking and visibility.
- Autosave.
- Project open/save dialogs.
- Recoverable error handling.

Deliverable: the application supports a complete editing session without data loss.

## Phase 11 — Presentation and export

Implement:

- Better lighting and shadows.
- Material and texture controls.
- Day/night presets.
- Screenshot export.
- Project JSON export/import.
- GLB export for basic scene sharing.
- Floor-plan image or PDF export.

Deliverable: a user can create and share a finished room design.

## Phase 12 — Performance and release quality

Measure and improve:

- Asset loading time.
- GPU memory usage.
- Frame time with 100–300 objects.
- Large-room behavior.
- Instancing and level of detail.
- Texture compression.
- Crash reporting and logs.
- Accessibility and keyboard navigation.

Deliverable: a stable MVP release for macOS and Linux.

## First vertical slice

The first end-to-end milestone should be:

1. Create one rectangular room.
2. Render its walls and floor in 3D.
3. Change a wall length in 2D.
4. See the 3D geometry update immediately.
5. Place one chair.
6. Select and move the chair.
7. Save and reload the project.

Do not begin with multiplayer, a large asset marketplace, photorealistic rendering, mobile support, or a custom scripting language.

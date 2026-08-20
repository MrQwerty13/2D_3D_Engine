# Technology Stack

## Target platforms

- Primary: macOS and Linux desktop.
- Architecture: native C++ engine with a platform-independent core.
- Build targets: Apple Silicon, Intel macOS where practical, and common 64-bit Linux distributions.

## Language and build

- **C++20** — engine and application code.
- **CMake** — project generation and dependency management.
- **Ninja** — fast local builds.
- **Clang** — primary compiler on macOS and Linux.
- **GCC** — supported Linux compiler.
- **clang-format** — source formatting.
- **clang-tidy** — static analysis.

## Platform layer

- **SDL3** — windows, input, keyboard, mouse, controllers, audio, and platform integration.
- Keep platform-specific code behind interfaces in `engine/platform/`.
- Use native Metal integration only where it provides a clear macOS-specific benefit.

## Rendering

- **bgfx** — recommended cross-platform rendering abstraction.
- **Metal** — macOS graphics backend.
- **Vulkan** — Linux graphics backend.
- Avoid making OpenGL the long-term primary backend because OpenGL is deprecated on macOS.
- Expose an engine-level renderer API so the rest of the code does not depend directly on bgfx.

## Mathematics and geometry

- **GLM** — vectors, matrices, quaternions, and common 3D math.
- Internal units: meters.
- Coordinate convention: right-handed world coordinates, Y-up.
- 2D coordinates: X/Z on the floor plane; Y is vertical in 3D.
- Procedural geometry for walls, floors, ceilings, doors, and windows.

## Assets and materials

- **glTF/GLB** — primary runtime format for furniture and 3D assets.
- **cgltf** or **Assimp** — model importing.
- **stb_image** — simple image loading.
- PBR material model with base color, metallic, roughness, normal, and emissive maps.
- Store catalog metadata separately from model files.

## User interface and editor

- **Dear ImGui** — first editor UI and debug tools.
- Add a native or custom polished UI layer after the engine workflow is stable.
- Use a 2D orthographic viewport for floor-plan editing.
- Use a 3D perspective viewport for room visualization.

## Data and persistence

- JSON for readable project files and early development.
- Version every project file.
- Add migration functions when the schema changes.
- Store stable IDs for rooms, walls, openings, furniture, assets, and materials.
- Use a command history for undo and redo.

## Testing and quality

- Unit tests for math, geometry, snapping, validation, and serialization.
- Integration tests for project save/load and 2D-to-3D generation.
- Visual regression tests for procedural room geometry.
- AddressSanitizer and UndefinedBehaviorSanitizer in debug CI.
- RenderDoc for Vulkan/Linux graphics debugging.
- Xcode Instruments and Metal debugging tools for macOS.

## Suggested directory layout

```text
engine/
  core/
  math/
  scene/
  renderer/
  platform/
  resources/
  input/
  audio/
  serialization/

editor/
  floorplan/
  viewport3d/
  tools/
  panels/

room_design/
  model/
  geometry/
  catalog/
  commands/
  validation/

tests/
assets/
third_party/
```

## Architecture rule

The room-design domain must not include renderer-specific types. It should produce plain data and geometry descriptions that can be consumed by the 2D and 3D renderers.

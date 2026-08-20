# 2D/3D Engine

A cross-platform C++ engine for building 2D and 3D applications, starting with
a room-design application for macOS and Linux.

The engine is designed around one shared core rather than separate 2D and 3D
engines. It will support 2D floor-plan editing and real-time 3D visualization
of rooms, walls, doors, windows, materials, and furniture.

## Project status

This repository is currently in the planning and foundation stage. The first
target is a small end-to-end vertical slice:

1. Create a rectangular room.
2. Render the room in 3D.
3. Edit a wall in 2D.
4. Update the 3D geometry immediately.
5. Place and move one furniture object.
6. Save and reload the project.

## Design direction

The 2D floor plan is the source of truth. The 3D scene is generated from the
validated room model.

```text
Room model
    │
    ├── 2D floor-plan renderer/editor
    │
    └── Procedural 3D room generator and renderer
```

This makes room dimensions, snapping, openings, furniture placement, undo/redo,
and serialization consistent across both views.

## Planned features

- 2D sprites, shapes, text, and tilemaps
- 3D meshes, materials, lights, and cameras
- Room walls, floors, ceilings, doors, and windows
- Grid, endpoint, corner, and wall snapping
- Furniture catalog using GLB/glTF assets
- Orthographic 2D and perspective 3D cameras
- Object selection, manipulation, and raycasting
- Undo and redo through command history
- Versioned project files
- Screenshot, floor-plan, and basic 3D export
- macOS and Linux support

## Technology stack

- **C++20** — engine and application code
- **Make** — build system
- **Clang++** — the project compiler
- **SDL3** — windowing, input, audio, and platform integration
- **bgfx** — cross-platform rendering abstraction
- **Metal** — macOS graphics backend
- **Vulkan** — Linux graphics backend
- **GLM** — mathematics
- **Dear ImGui** — initial editor interface
- **glTF/GLB** — runtime 3D asset format
- **JSON** — readable, versioned project files

OpenGL is not planned as the long-term primary backend because it is deprecated
on macOS. The renderer should hide backend-specific details from the engine and
room-design layers.

## Architecture

```text
Engine core
 ├── Math and transforms
 ├── Entity/scene system
 ├── Resources and asset cache
 ├── Input and timing
 ├── Serialization
 └── Platform abstraction

Rendering
 ├── 2D renderer
 │    ├── Sprites
 │    ├── Shapes
 │    ├── Text
 │    └── Tilemaps
 │
 └── 3D renderer
      ├── Meshes
      ├── Materials
      ├── Lighting
      ├── Cameras
      └── Shadows

Room design
 ├── Room model
 ├── Wall and opening geometry
 ├── Furniture catalog
 ├── Snapping and validation
 └── Editing commands
```

The room-design domain must remain independent from renderer-specific types.
The engine uses meters internally, with X/Z representing the floor plane and Y
representing vertical space.

## Planned repository structure

```text
engine/        Shared engine systems and rendering
editor/        2D/3D editor tools and panels
room_design/   Room model, geometry, catalog, and validation
tests/         Unit and integration tests
assets/        Development models, textures, and fonts
third_party/   External dependencies
```

## Development roadmap

Development is divided into the following phases:

1. Product definition
2. Make and platform foundation
3. Engine core
4. Rendering foundation
5. 2D engine
6. 3D engine
7. Room-design domain model
8. 2D floor-plan editor
9. Procedural 3D room generation
10. Furniture catalog
11. Editing workflow
12. Presentation, export, performance, and release

See [ROADMAP.md](ROADMAP.md) for details.

See [TECHSTACK.md](TECHSTACK.md) for the technology and architecture decisions.

See [PROMTS_FOR_AI.md](PROMTS_FOR_AI.md) for prompts that can be used to develop
the engine phase by phase.

## Initial development requirements

The project will target:

- macOS with Clang++
- Linux with Clang++
- Debug and Release builds
- Unit tests for math, geometry, snapping, validation, and serialization

SDL3 must be installed and discoverable through `pkg-config`.

## Building the foundation

The project uses a Makefile and always compiles with `clang++`. SDL3 is the
only external dependency in this foundation and must be installed separately.

Debug build and tests:

```sh
make debug
make test
```

Release build and tests:

```sh
make release
```

On macOS, install LLVM/Apple Clang and SDL3 with Homebrew. On Linux, install
Clang, Make, SDL3, and the SDL3 development package using the distribution's
package manager. Verify that `pkg-config --modversion sdl3` succeeds.

The `format` target runs the repository's `.clang-format` configuration:

```sh
make format
```

The initial executable creates an SDL window and processes close events. It
does not initialize bgfx or perform any rendering yet.

## License

See [LICENSE](LICENSE).

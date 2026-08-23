# 2D/3D Engine

A cross-platform C++ engine for building 2D and 3D applications, starting with
a room-design application for macOS and Linux.

The engine is designed around one shared core rather than separate 2D and 3D
engines. It will support 2D floor-plan editing and real-time 3D visualization
of rooms, walls, doors, windows, materials, and furniture.

## Project status

The repository now contains the first end-to-end vertical slice plus
presentation/export and profiling tooling:

1. Create a rectangular room.
2. Render the room in 3D.
3. Edit a wall in 2D.
4. Update the 3D geometry immediately.
5. Place and move one furniture object.
6. Save and reload the project.
7. Export project JSON, basic GLB geometry, SVG floor plans, and null-backend screenshots.

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

Presentation and release notes are in [docs/GLB_LIMITATIONS.md](docs/GLB_LIMITATIONS.md),
[docs/PERFORMANCE.md](docs/PERFORMANCE.md), and
[docs/RELEASE_CHECKLIST.md](docs/RELEASE_CHECKLIST.md). Use `make CONFIG=Release profile`
to measure procedural build time and project size for 100–300 furniture objects.

## Reusing the engine

Build reusable component libraries with `make CONFIG=Debug libraries` or
`make CONFIG=Release libraries`. Separate furniture-editor applications can
include `room_engine/furniture_editor.hpp` and link the renderer library; the
optional platform library owns SDL window/application setup. See
[docs/INTEGRATION.md](docs/INTEGRATION.md) and run `make example` for a complete
consumer that creates matching 2D and 3D furniture scenes.

## Initial development requirements

The project will target:

- macOS with Clang++
- Linux with Clang++
- Debug and Release builds
- Unit tests for math, geometry, snapping, validation, and serialization

SDL3 must be installed and discoverable through `pkg-config`.

## Building the foundation

The project uses Make and always compiles with `clang++`. SDL3 is the only
external dependency in this foundation. Its exact `pkg-config` version is
recorded in [dependencies.lock](dependencies.lock), and the Makefile refuses
to build against a different version until the lock is deliberately updated.

Install SDL3 3.4.14 and verify that it is visible:

```sh
pkg-config --modversion sdl3   # must print 3.4.14
```

On macOS with Homebrew:

```sh
brew install llvm pkg-config sdl3
export PATH="$(brew --prefix llvm)/bin:$PATH"
```

On Debian/Ubuntu-like Linux distributions, use a repository that provides
SDL3 development files, then install Clang, Make, and pkg-config alongside it:

```sh
sudo apt install clang make pkg-config libsdl3-dev
```

If the distribution package has another version, install SDL3 3.4.14 from
source or update both `dependencies.lock` and `SDL3_VERSION` in the Makefile
as a reviewed dependency change.

Debug build and tests:

```sh
make debug
make test
```

Release build and tests:

```sh
make release
make CONFIG=Release test
```

On macOS, install LLVM/Apple Clang and SDL3 with Homebrew. On Linux, install
Clang, Make, SDL3, and the SDL3 development package using the distribution's
package manager. Verify that `pkg-config --modversion sdl3` succeeds.

The `format` target runs the repository's `.clang-format` configuration, and
`make check` runs the Debug smoke test plus formatting validation:

```sh
make format
make check
```

The executable owns the SDL application lifecycle and submits a triangle plus
debug grid and axes through the renderer abstraction. The default build uses a
small no-op renderer when bgfx is unavailable, which keeps tests and engine
tools buildable without a graphics SDK. To enable the bgfx backend, provide
bgfx compiler/linker flags (for example, from a local bgfx build):

```sh
make BGFX_CFLAGS="-I/path/to/bgfx/include -I/path/to/bx/include" \
     BGFX_LIBS="-L/path/to/bgfx/lib -lbgfx -lbx -lbimg" build
```

The shader manager accepts bgfx shader binaries produced from
`assets/shaders/debug.vs.sc` and `debug.fs.sc` with bgfx's `shaderc` tool. The
renderer API only exposes engine-owned handles and data types; bgfx types are
confined to `renderer.cpp`.

## License

See [LICENSE](LICENSE).

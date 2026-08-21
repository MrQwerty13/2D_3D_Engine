# Using room_engine from smaller projects

The engine exposes three layers:

- `room_engine/core.hpp` is header-only room data, transforms, serialization,
  validation, and floor-plan editing. It does not require SDL at link time.
- `room_engine/rendering.hpp` plus `libroom_engine_renderer.a` provides the 2D
  and 3D renderers, room generation, asset loading, and exports.
- `room_engine/application.hpp` plus `libroom_engine_platform.a` provides the
  optional SDL application/window lifecycle.

For a furniture editor, include `room_engine/furniture_editor.hpp`. Call
`populate_furniture_floor_plan` for the top-down view and
`populate_furniture_scene` for 3D. Both consume the same `RoomDesign`, so IDs,
dimensions, materials, project files, and furniture placement stay consistent.

## Build the libraries

From the engine repository:

```sh
make CONFIG=Debug libraries
make CONFIG=Release libraries
```

Outputs:

```text
out/debug/lib/libroom_engine_renderer.a
out/debug/lib/libroom_engine_platform.a
out/release/lib/libroom_engine_renderer.a
out/release/lib/libroom_engine_platform.a
```

## Link from a sibling project

This minimal Makefile fragment assumes the engine and editor repositories have
the same parent directory:

```make
ENGINE_ROOT ?= ../2D_3D_Engine
ENGINE_CONFIG ?= debug
CXX := clang++
CPPFLAGS += -I$(ENGINE_ROOT)/src $(shell pkg-config --cflags sdl3)
CXXFLAGS += -std=c++20
LDLIBS += $(ENGINE_ROOT)/out/$(ENGINE_CONFIG)/lib/libroom_engine_renderer.a
LDLIBS += $(shell pkg-config --libs sdl3)

editor: editor.cpp
	$(MAKE) -C $(ENGINE_ROOT) CONFIG=Debug libraries
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDLIBS) -o $@
```

Add `libroom_engine_platform.a` before `libroom_engine_renderer.a` when the
smaller application uses `room_engine::Application`. Static-library order is
significant on Linux.

If bgfx is enabled, pass the same `BGFX_CFLAGS` and `BGFX_LIBS` when building
the engine and consumer. On macOS, also link the frameworks listed in the
engine Makefile. Do not mix Debug and Release objects or libraries.

## Example

Run the non-windowed integration example:

```sh
make CONFIG=Debug example
```

The complete source is `examples/furniture_editor_integration.cpp`.

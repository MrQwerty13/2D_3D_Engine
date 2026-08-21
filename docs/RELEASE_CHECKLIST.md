# Release checklist

## Both platforms

- Run `make CONFIG=Release build`, `make CONFIG=Release test`, and `make check`.
- Run the profile harness and archive its output with the build.
- Verify JSON round-trip, GLB load-back, SVG floor plan, and screenshot export.
- Test malformed/truncated JSON, GLB, and asset files.

## macOS

- Test with Apple Clang and SDL3 on the minimum supported macOS.
- Verify Metal creation, resize, screenshot orientation, and packaging.
- Run Instruments for CPU, allocations, and GPU counters.

## Linux

- Test with Clang, SDL3, and the selected Vulkan driver.
- Check X11/Wayland policy and native window-handle paths.
- Run perf/Tracy and RenderDoc where available.

Known limitations: screenshot export is implemented by the SDL null renderer;
the bgfx backend reports unsupported readback. GLB is geometry-only; see
`docs/GLB_LIMITATIONS.md`.

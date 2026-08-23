# Performance profiling

Run `make CONFIG=Release profile`. The harness generates large rooms with 100,
200, and 300 furniture objects and reports procedural scene-build CPU time,
instance count, and serialized project bytes. It does not claim GPU frame time,
GPU memory, or OS-wide CPU usage: those require a real backend and an external
profiler (Xcode Instruments/Metal System Trace on macOS; RenderDoc, Tracy, or
perf plus the selected Vulkan backend on Linux).

The existing GLTF cache avoids repeated asset I/O. Instancing, texture
compression, and LOD remain disabled until real asset/backend measurements
justify them. The main remaining hot path is per-frame transformed vertex
upload in `Renderer3D::flush`; measure it before changing batching semantics.

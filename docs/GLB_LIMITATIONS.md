# GLB export limitations

The basic exporter writes procedural room geometry as one glTF 2.0 mesh. It is
intended for interchange and review, not as a lossless project archive.

GLB cannot represent the room editor's stable IDs, wall/opening relationships,
undo/redo history, snapping state, visibility/lock state, catalog provenance,
custom room metadata, or the distinction between a procedural wall and its
opening parameters. Current export also omits textures, material names and
lighting/camera/editor presentation settings. Keep the versioned project JSON
as the source of truth when those values matter.

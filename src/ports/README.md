# Shared port support

This directory contains portable support code used by the platform ports,
kept separate from the reconstructed game systems in `src/system`.

`pecompact_memory.cpp` and `.hpp` provide one in-memory decoder for the
supported original Steam `OR2006C2C.EXE`. Switch, macOS, PS5, Xbox and host
setup tools compile this same implementation. The loader in
`src/system/exe_image.cpp` calls it and verifies the expected data ranges.

The decoder returns a data image indexed by RVA. It does not execute the
PC file, resolve its imports or allocate executable memory. Game files,
extracted images and private validation artifacts are not stored here.

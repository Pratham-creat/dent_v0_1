# DENT v0.1

First prototype of the DENT sovereign optimization engine.

## Current scope

- C++20
- Basic LP problem representation
- Objective function
- Constraint representation
- Initial sparse matrix component
- CLI executable

## Build

### Linux / macOS

```bash
cmake -S . -B build
cmake --build build
./build/dent
```

### Windows

```powershell
cmake -S . -B build
cmake --build build
.\build\Debug\dent.exe
```

## Next milestone

Implement the revised/primal simplex solver on top of this model representation.

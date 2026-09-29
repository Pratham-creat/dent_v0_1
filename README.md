# DENT Optimization Engine

DENT is a C++20 mathematical optimization engine for LP, MILP, QP and MIQP models, with native C and Python bindings.

## Implemented engine

- LP / MILP / QP / MIQP model representation
- Primal simplex, dual simplex and interior-point methods
- PDHG and PDLP first-order methods
- MILP branch-and-bound with warm-start support, heuristics and cover-cut infrastructure
- MIQP branch-and-bound using QP relaxations
- Presolve, scaling, adaptive problem fingerprinting and solver dispatch
- Solution validation
- Warm-start/session APIs
- Sparse matrix representation
- LU factorization with partial pivoting and Markowitz-style pivot selection
- Primal feasibility, Farkas-style infeasibility and unbounded-ray certificate verification
- Native `.dent`, MPS/QPS and LP ingestion
- Native C API and a thin Python ctypes API
- Optional CUDA build with CSR SpMV / vector kernels
- CMake install rules, Linux + Windows CI and ASan/UBSan CI

## Build

### Windows / Visual Studio

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.uildReleasedent.exe
```

### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/dent
```

### Optional CUDA backend

CUDA is opt-in and is never reported as active unless it is explicitly built:

```bash
cmake -S . -B build -DDENT_ENABLE_CUDA=ON
cmake --build build --parallel
```

## Model formats

- `.dent`: DENT native format
- `.mps` / `.qps`: MPS-family parser
- `.lp`: LP-style text parser

## Python

After building the native API, set `DENT_LIBRARY` to the generated shared library and use:

```python
from python.dent import Model

m = Model(maximize=True)
x = m.add_variable("x", 0, 10)
m.set_objective(x, 1)
print(m.solve())
```

## Important scope

The engine now contains real implementations for the listed subsystems, but this is not a claim of numerical/performance equivalence with commercial solvers. Large-scale sparse factorization, CUDA execution integration, and advanced MILP cut/branching strategies remain areas where benchmark evidence is required before making performance claims.

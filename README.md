# DENT Optimization Engine Frontend

React + Vite frontend for the DENT Optimization Engine.

## Run locally

From the repository root:

```powershell
cd frontend
npm install
npm run dev
```

The frontend expects the FastAPI backend at `http://127.0.0.1:8000` by default.

Set `VITE_API_URL` when the API is deployed elsewhere.

## Implemented

- Figma-aligned dark optimization workstation UI
- Live solver configuration
- JSON solve integration with `POST /api/v1/solve`
- Native API health indicator
- Run history
- Solver result/fingerprint display
- Objective convergence visualization from persisted run objectives
- Responsive density adjustments


## Model input and test cases

Open **model input** from the top navigation.

There are three input paths:

1. **Test library** — select a built-in model and run it.
2. **JSON editor** — paste/edit a `SolveRequest`, click **load JSON**, then **run loaded model**.
3. **.dent file** — choose a `.dent` file and click **solve file**.

Built-in cases currently include:

- production planning (LP)
- minimum allocation (LP)
- integer production (MILP)
- small quadratic program (QP)
- infeasible constraints (LP)

All JSON/test-library cases call `POST /api/v1/solve`. File cases call `POST /api/v1/solve-file`.

### Run locally

Terminal 1 — backend:

```powershell
cd D:\Dent\dent_v0_1
.\.venv\Scripts\Activate.ps1
$env:DENT_API_LIBRARY="$PWD\dent_v0_1\build\Release\dent_api.dll"
Test-Path $env:DENT_API_LIBRARY
python backend\main.py
```

The last command should start FastAPI on `http://127.0.0.1:8000`.

Terminal 2 — frontend:

```powershell
cd D:\Dent\dent_v0_1\frontend
npm install
npm run dev
```

Open `http://localhost:5173`.

### Recommended first test

1. Click **model input**.
2. Leave **test library** selected.
3. Select **integer production**.
4. Click **run test case**.
5. The request is sent to DENT's native solver through FastAPI.
6. The result appears in the workbench and is persisted in **history**.

For the integer-production test, the known DENT result is an optimal objective of `2533` with BatchA=5, BatchB=9, BatchC=3, BatchD=46 when the current native solver behavior matches the existing regression case.

### Custom JSON example

```json
{
  "objective": "maximize",
  "variables": [
    {
      "name": "x",
      "lower_bound": 0,
      "upper_bound": 0,
      "type": "continuous",
      "objective_coefficient": 3
    },
    {
      "name": "y",
      "lower_bound": 0,
      "upper_bound": 0,
      "type": "continuous",
      "objective_coefficient": 2
    }
  ],
  "constraints": [
    {
      "name": "capacity",
      "sense": 0,
      "rhs": 10,
      "coefficients": {
        "x": 2,
        "y": 1
      }
    }
  ],
  "quadratic_terms": [],
  "solver": {
    "method": "primal_simplex",
    "tolerance": 0,
    "max_iterations": 0
  }
}
```

This LP has the expected mathematical optimum `15` at `x=5, y=0`.

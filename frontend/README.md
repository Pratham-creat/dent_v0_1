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

# DENT REST API

FastAPI wrapper around the native DENT C API.

## Endpoints

- GET /health — native API health.
- GET /api/v1/capabilities — engine, solver, model, configuration, benchmark, and telemetry capabilities.
- POST /api/v1/solve — solve a JSON optimization model.
- POST /api/v1/solve-file — solve a .dent model file.
- POST /api/v1/benchmark — execute the same model with multiple configured solver methods and capture objective, iterations, and solve time.
- GET /api/v1/runs — list persisted runs.
- GET /api/v1/runs/{run_id} — retrieve a persisted run.
- GET /api/v1/runs/telemetry — aggregate solve telemetry from SQLite history.
- DELETE /api/v1/runs/{run_id} — delete a run.
- DELETE /api/v1/runs — clear run history.

## Telemetry

Each successful solve records solve_time_ms alongside the solver result. Run telemetry aggregates total/optimal runs, solve-time statistics, average iterations, source counts, and per-solver metrics.

## Benchmarking

The benchmark endpoint accepts one model and a list of solver methods. Each successful method execution is persisted in run history and the response reports its run ID, objective, iterations, selected native solver, and measured solve time. A method failure is reported for that method without discarding other completed benchmark results.

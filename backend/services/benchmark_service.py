from fastapi import HTTPException

from api.schemas.model import BenchmarkRequest
from services.solver_service import solver_service


class BenchmarkService:
    def run(self, request: BenchmarkRequest):
        results = []

        for method in dict.fromkeys(request.methods):
            configured = request.model.model_copy(
                update={
                    "solver": request.model.solver.model_copy(
                        update={"method": method}
                    )
                }
            )

            try:
                run = solver_service.solve(configured)
                result = run["result"]
                results.append({
                    "method": method,
                    "status": "completed",
                    "run_id": run["id"],
                    "solver": result.get("solver"),
                    "objective": result.get("objective"),
                    "iterations": result.get("iterations"),
                    "solve_time_ms": result.get("solve_time_ms"),
                })
            except HTTPException as exc:
                results.append({
                    "method": method,
                    "status": "failed",
                    "error": str(exc.detail),
                    "code": (exc.headers or {}).get(
                        "X-DENT-Error-Code", "SOLVER_ERROR"
                    ),
                })

        completed = [item for item in results if item["status"] == "completed"]
        return {
            "requested_methods": list(dict.fromkeys(request.methods)),
            "completed": len(completed),
            "failed": len(results) - len(completed),
            "results": results,
        }


benchmark_service = BenchmarkService()

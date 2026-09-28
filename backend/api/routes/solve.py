from pathlib import Path

from fastapi import APIRouter, File, HTTPException, Query, UploadFile

from api.schemas.errors import ErrorResponse
from api.schemas.model import BenchmarkRequest, RunResponse, SolveRequest, SolverOptions
from services.benchmark_service import benchmark_service
from services.solver_service import solver_service


router = APIRouter(prefix="/api/v1")

ERROR_RESPONSES = {
    400: {"model": ErrorResponse, "description": "Invalid optimization request."},
    422: {"model": ErrorResponse, "description": "Request schema validation failed."},
    500: {"model": ErrorResponse, "description": "Solver execution failed."},
    503: {"model": ErrorResponse, "description": "DENT native API is unavailable."},
}


@router.post(
    "/solve",
    response_model=RunResponse,
    responses=ERROR_RESPONSES,
)
def solve(request: SolveRequest):
    return solver_service.solve(request)


@router.post("/benchmark")
def benchmark(request: BenchmarkRequest):
    return benchmark_service.run(request)


@router.post(
    "/solve-file",
    response_model=RunResponse,
    responses=ERROR_RESPONSES,
)
async def solve_file(
    file: UploadFile = File(...),
    method: str = Query("auto"),
    tolerance: float = Query(0.0, ge=0.0),
    max_iterations: int = Query(0, ge=0),
):
    if not file.filename or Path(file.filename).suffix.lower() != ".dent":
        raise HTTPException(
            400,
            "Only .dent model files are supported.",
            headers={"X-DENT-Error-Code": "INVALID_FILE_TYPE"},
        )

    try:
        options = SolverOptions(
            method=method,
            tolerance=tolerance,
            max_iterations=max_iterations,
        )
    except Exception as exc:
        raise HTTPException(
            status_code=400,
            detail=str(exc),
            headers={"X-DENT-Error-Code": "INVALID_SOLVER_OPTIONS"},
        ) from exc

    return solver_service.solve_file(
        await file.read(),
        file.filename,
        options,
    )

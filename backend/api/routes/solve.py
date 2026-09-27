from pathlib import Path

from fastapi import APIRouter, File, HTTPException, UploadFile

from api.schemas.errors import ErrorResponse
from api.schemas.model import SolveRequest
from services.solver_service import solver_service


router = APIRouter(prefix="/api/v1")

ERROR_RESPONSES = {
    400: {"model": ErrorResponse, "description": "Invalid optimization request."},
    422: {"model": ErrorResponse, "description": "Request schema validation failed."},
    500: {"model": ErrorResponse, "description": "Solver execution failed."},
    503: {"model": ErrorResponse, "description": "DENT native API is unavailable."},
}


@router.post("/solve", responses=ERROR_RESPONSES)
def solve(request: SolveRequest):
    return solver_service.solve(request)


@router.post("/solve-file", responses=ERROR_RESPONSES)
async def solve_file(file: UploadFile = File(...)):
    if not file.filename or Path(file.filename).suffix.lower() != ".dent":
        raise HTTPException(
            400,
            "Only .dent model files are supported.",
            headers={"X-DENT-Error-Code": "INVALID_FILE_TYPE"},
        )

    return solver_service.solve_file(await file.read(), file.filename)

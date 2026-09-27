from pathlib import Path

from fastapi import APIRouter, File, HTTPException, UploadFile

from api.schemas.model import SolveRequest
from services.solver_service import solver_service


router = APIRouter(prefix="/api/v1")


@router.post("/solve")
def solve(request: SolveRequest):
    return solver_service.solve(request)


@router.post("/solve-file")
async def solve_file(file: UploadFile = File(...)):
    if not file.filename or Path(file.filename).suffix.lower() != ".dent":
        raise HTTPException(400, "Only .dent model files are supported.")

    return solver_service.solve_file(await file.read(), file.filename)

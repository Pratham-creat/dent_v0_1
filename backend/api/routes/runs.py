from fastapi import APIRouter, HTTPException, Query, Response, status

from api.schemas.errors import ErrorResponse
from services.run_service import run_service


router = APIRouter(prefix="/api/v1/runs", tags=["runs"])

ERROR_RESPONSES = {
    404: {"model": ErrorResponse, "description": "Run not found."},
}


@router.get("")
def list_runs(
    limit: int = Query(50, ge=1, le=100),
    offset: int = Query(0, ge=0),
):

    return {
        "runs": run_service.list(limit=limit, offset=offset),
        "limit": limit,
        "offset": offset,
    }


@router.delete("", status_code=status.HTTP_204_NO_CONTENT)
def clear_runs():
    run_service.clear()
    return Response(status_code=status.HTTP_204_NO_CONTENT)


@router.get("/{run_id}", responses=ERROR_RESPONSES)
def get_run(run_id: str):
    run = run_service.get(run_id)
    if run is None:
        raise HTTPException(
            status_code=404,
            detail="Run not found",
            headers={"X-DENT-Error-Code": "RUN_NOT_FOUND"},
        )
    return run


@router.delete("/{run_id}", status_code=status.HTTP_204_NO_CONTENT, responses=ERROR_RESPONSES)
def delete_run(run_id: str):
    if not run_service.delete(run_id):
        raise HTTPException(
            status_code=404,
            detail="Run not found",
            headers={"X-DENT-Error-Code": "RUN_NOT_FOUND"},
        )
    return Response(status_code=status.HTTP_204_NO_CONTENT)

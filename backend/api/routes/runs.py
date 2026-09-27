from fastapi import APIRouter, HTTPException, Query, Response, status

from services.run_service import run_service


router = APIRouter(prefix="/api/v1/runs", tags=["runs"])


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


@router.get("/{run_id}")
def get_run(run_id: str):
    run = run_service.get(run_id)
    if run is None:
        raise HTTPException(status_code=404, detail="Run not found")
    return run


@router.delete("/{run_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_run(run_id: str):
    if not run_service.delete(run_id):
        raise HTTPException(status_code=404, detail="Run not found")
    return Response(status_code=status.HTTP_204_NO_CONTENT)

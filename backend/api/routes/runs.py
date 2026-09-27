from fastapi import APIRouter, HTTPException, Query

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


@router.get("/{run_id}")
def get_run(run_id: str):
    run = run_service.get(run_id)
    if run is None:
        raise HTTPException(status_code=404, detail="Run not found")
    return run

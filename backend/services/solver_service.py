from fastapi import HTTPException

from api.schemas.model import SolveRequest
from native.dent_api import DentAPI


class SolverService:
    def __init__(self):
        self._native = None

    def _get_native(self):
        if self._native is None:
            try:
                self._native = DentAPI()
            except Exception as exc:
                raise HTTPException(status_code=503, detail=str(exc)) from exc
        return self._native

    def solve(self, request: SolveRequest):
        try:
            return self._get_native().solve(request)
        except ValueError as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc

    def solve_file(self, data: bytes):
        try:
            return self._get_native().solve_file(data)
        except ValueError as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc

    def health(self):
        try:
            api = DentAPI()
            return {
                "status": "ok",
                "native_api": True,
                "library": str(api.path),
            }
        except Exception as exc:
            return {
                "status": "degraded",
                "native_api": False,
                "error": str(exc),
            }


solver_service = SolverService()

from fastapi import HTTPException

from api.schemas.model import SolveRequest
from native.dent_api import DentAPI
from services.run_service import run_service
from services.model_validation import validate_solve_request


class SolverService:
    def __init__(self):
        self._native = None

    def _get_native(self):
        if self._native is None:
            try:
                self._native = DentAPI()
            except (FileNotFoundError, OSError, RuntimeError) as exc:
                raise HTTPException(
                    status_code=503,
                    detail="DENT native API is unavailable.",
                    headers={"X-DENT-Error-Code": "NATIVE_API_UNAVAILABLE"},
                ) from exc
        return self._native

    def solve(self, request: SolveRequest):
        validate_solve_request(request)
        try:
            result = self._get_native().solve(request)
            return run_service.create(result, "json")
        except HTTPException:
            raise
        except ValueError as exc:
            raise HTTPException(
                status_code=400,
                detail=str(exc),
                headers={"X-DENT-Error-Code": "MODEL_ERROR"},
            ) from exc
        except (RuntimeError, OSError) as exc:
            raise HTTPException(
                status_code=500,
                detail="DENT solver execution failed.",
                headers={"X-DENT-Error-Code": "SOLVER_RUNTIME_ERROR"},
            ) from exc

    def solve_file(self, data: bytes, source_name=None):
        try:
            result = self._get_native().solve_file(data)
            return run_service.create(result, "file", source_name)
        except HTTPException:
            raise
        except ValueError as exc:
            raise HTTPException(
                status_code=400,
                detail=str(exc),
                headers={"X-DENT-Error-Code": "MODEL_FILE_ERROR"},
            ) from exc
        except (RuntimeError, OSError) as exc:
            raise HTTPException(
                status_code=500,
                detail="DENT solver execution failed.",
                headers={"X-DENT-Error-Code": "SOLVER_RUNTIME_ERROR"},
            ) from exc

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

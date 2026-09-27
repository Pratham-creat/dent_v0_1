from fastapi import HTTPException

from api.schemas.model import SolveRequest
from native.dent_api import DentAPI
from services.model_validation import validate_solve_request
from services.run_service import run_service


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

    @staticmethod
    def _raise_normalized(exc: HTTPException, default_code: str):
        headers = dict(exc.headers or {})
        headers.setdefault("X-DENT-Error-Code", default_code)
        raise HTTPException(
            status_code=exc.status_code,
            detail=exc.detail,
            headers=headers,
        ) from exc

    def solve(self, request: SolveRequest):
        try:
            validate_solve_request(request)
            result = self._get_native().solve(request)
            result["configuration"] = request.solver.model_dump()
            return run_service.create(result, "json")
        except HTTPException as exc:
            self._raise_normalized(exc, "MODEL_VALIDATION_ERROR")
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

    def solve_file(self, data: bytes, source_name=None, options=None):
        try:
            result = self._get_native().solve_file(data, options)
            if options is not None:
                result["configuration"] = options.model_dump()
            return run_service.create(result, "file", source_name)
        except HTTPException as exc:
            self._raise_normalized(exc, "MODEL_FILE_ERROR")
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

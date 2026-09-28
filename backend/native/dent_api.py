from pathlib import Path
import ctypes
import json
import os
import platform
import tempfile

from fastapi import HTTPException


ROOT = Path(__file__).resolve().parents[2]


def _library_candidates():
    override = os.getenv("DENT_API_LIBRARY")
    if override:
        return [Path(override)]

    system = platform.system().lower()
    name = (
        "dent_api.dll"
        if "windows" in system
        else "libdent_api.dylib"
        if "darwin" in system
        else "libdent_api.so"
    )
    return [ROOT / p / name for p in ("build", "build/Release", "build/Debug")]


class DentAPI:
    def __init__(self):
        self.path = next((p for p in _library_candidates() if p.exists()), None)
        if not self.path:
            raise FileNotFoundError(
                "DENT native API library not found. "
                "Build dent_api or set DENT_API_LIBRARY."
            )

        self.lib = ctypes.CDLL(str(self.path))
        self._bind()

    def _bind(self):
        library = self.lib

        library.dent_model_create.argtypes = [ctypes.c_int]
        library.dent_model_create.restype = ctypes.c_void_p

        library.dent_model_destroy.argtypes = [ctypes.c_void_p]

        library.dent_model_add_variable.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.c_double,
            ctypes.c_double,
            ctypes.c_int,
        ]
        library.dent_model_add_variable.restype = ctypes.c_int

        library.dent_model_add_constraint.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.c_int,
            ctypes.c_double,
        ]
        library.dent_model_add_constraint.restype = ctypes.c_int

        library.dent_model_set_objective_coefficient.argtypes = [
            ctypes.c_void_p,
            ctypes.c_int,
            ctypes.c_double,
        ]
        library.dent_model_set_objective_coefficient.restype = ctypes.c_int

        library.dent_model_set_constraint_coefficient.argtypes = [
            ctypes.c_void_p,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_double,
        ]
        library.dent_model_set_constraint_coefficient.restype = ctypes.c_int

        library.dent_model_set_quadratic_coefficient.argtypes = [
            ctypes.c_void_p,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_double,
        ]
        library.dent_model_set_quadratic_coefficient.restype = ctypes.c_int

        library.dent_model_set_variable_type.argtypes = [
            ctypes.c_void_p,
            ctypes.c_int,
            ctypes.c_int,
        ]
        library.dent_model_set_variable_type.restype = ctypes.c_int

        library.dent_model_solve_json.argtypes = [ctypes.c_void_p]
        library.dent_model_solve_json.restype = ctypes.c_void_p

        library.dent_model_solve_json_with_options.argtypes = [
            ctypes.c_void_p,
            ctypes.c_int,
            ctypes.c_double,
            ctypes.c_int,
        ]
        library.dent_model_solve_json_with_options.restype = ctypes.c_void_p

        library.dent_solve_file_json.argtypes = [ctypes.c_char_p]
        library.dent_solve_file_json.restype = ctypes.c_void_p

        library.dent_solve_file_json_with_options.argtypes = [
            ctypes.c_char_p,
            ctypes.c_int,
            ctypes.c_double,
            ctypes.c_int,
        ]
        library.dent_solve_file_json_with_options.restype = ctypes.c_void_p

        library.dent_last_error.restype = ctypes.c_char_p
        library.dent_free_string.argtypes = [ctypes.c_void_p]

    def error(self):
        raw = self.lib.dent_last_error()
        return raw.decode("utf-8", "replace") if raw else "Unknown DENT error"

    def _decode(self, pointer):
        if not pointer:
            raise RuntimeError(self.error())

        try:
            raw = ctypes.cast(pointer, ctypes.c_char_p).value
            return json.loads(raw.decode("utf-8"))
        finally:
            self.lib.dent_free_string(pointer)

    @staticmethod
    def _solver_method(method):
        return {
            "auto": 0,
            "primal_simplex": 1,
            "interior_point": 3,
            "pdhg": 4,
            "pdlp": 5,
            "qp": 6,
            "milp": 7,
        }[method]

    def solve(self, request):
        model = self.lib.dent_model_create(
            1 if request.objective == "maximize" else 0
        )
        if not model:
            raise RuntimeError(self.error())

        try:
            variable_ids = {}
            variable_types = {"continuous": 0, "integer": 1, "binary": 2}

            for index, variable in enumerate(request.variables):
                result = self.lib.dent_model_add_variable(
                    model,
                    variable.name.encode(),
                    variable.lower_bound,
                    variable.upper_bound,
                    variable_types[variable.type],
                )
                if result < 0:
                    raise RuntimeError(self.error())

                variable_ids[variable.name] = index

                if self.lib.dent_model_set_objective_coefficient(
                    model, index, variable.objective_coefficient
                ):
                    raise RuntimeError(self.error())

            for index, constraint in enumerate(request.constraints):
                result = self.lib.dent_model_add_constraint(
                    model,
                    constraint.name.encode(),
                    constraint.sense,
                    constraint.rhs,
                )
                if result < 0:
                    raise RuntimeError(self.error())

                for name, coefficient in constraint.coefficients.items():
                    if name not in variable_ids:
                        raise HTTPException(400, f"Unknown variable: {name}")

                    if self.lib.dent_model_set_constraint_coefficient(
                        model,
                        index,
                        variable_ids[name],
                        coefficient,
                    ):
                        raise RuntimeError(self.error())

            for term in request.quadratic_terms:
                if term.row not in variable_ids or term.column not in variable_ids:
                    raise HTTPException(
                        400, "Unknown variable in quadratic term"
                    )

                if self.lib.dent_model_set_quadratic_coefficient(
                    model,
                    variable_ids[term.row],
                    variable_ids[term.column],
                    term.coefficient,
                ):
                    raise RuntimeError(self.error())

            options = request.solver
            return self._decode(
                self.lib.dent_model_solve_json_with_options(
                    model,
                    self._solver_method(options.method),
                    options.tolerance,
                    options.max_iterations,
                )
            )
        finally:
            self.lib.dent_model_destroy(model)

    def solve_file(self, data, options=None):
        path = None
        try:
            with tempfile.NamedTemporaryFile(delete=False, suffix=".dent") as file:
                file.write(data)
                path = file.name

            if options is None:
                return self._decode(
                    self.lib.dent_solve_file_json(path.encode())
                )

            return self._decode(
                self.lib.dent_solve_file_json_with_options(
                    path.encode(),
                    self._solver_method(options.method),
                    options.tolerance,
                    options.max_iterations,
                )
            )
        finally:
            if path:
                Path(path).unlink(missing_ok=True)

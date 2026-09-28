from __future__ import annotations

import ctypes
import json
import os
import platform
from pathlib import Path
from typing import Any


class DENTError(RuntimeError):
    """Raised when the DENT native API reports an error."""


class SolveResult(dict):
    """
    Dictionary-compatible DENT solve result.

    Existing code such as:

        result["objective"]

    continues to work.

    Convenience properties are also available:

        result.status
        result.objective
        result.solver
        result.variables
    """

    @property
    def status(self) -> str:
        return self["status"]

    @property
    def objective(self) -> float:
        return self["objective"]

    @property
    def solver(self) -> str:
        return self["solver"]

    @property
    def iterations(self) -> int:
        return self["iterations"]

    @property
    def message(self) -> str:
        return self["message"]

    @property
    def variables(self) -> list[dict[str, Any]]:
        return self["variables"]

    @property
    def fingerprint(self) -> dict[str, Any]:
        return self["fingerprint"]


def _library_candidates() -> list[Path]:
    package_dir = Path(__file__).resolve().parent
    project_dir = package_dir.parent.parent

    candidates: list[Path] = []

    if os.name == "nt":
        names = [
            "dent_api.dll",
        ]
    elif platform.system() == "Darwin":
        names = [
            "libdent_api.dylib",
        ]
    else:
        names = [
            "libdent_api.so",
        ]

    search_directories = [
        project_dir / "build",
        project_dir / "build" / "Release",
        project_dir / "build" / "Debug",
    ]

    for directory in search_directories:
        for name in names:
            candidates.append(
                directory / name
            )

    return candidates


def _find_library() -> Path:
    candidates = _library_candidates()

    for candidate in candidates:
        if candidate.is_file():
            return candidate

    searched = "\n".join(
        f"  - {candidate}"
        for candidate in candidates
    )

    raise DENTError(
        "Could not find the DENT native API library.\n"
        "Build the project first.\n\n"
        "Searched:\n"
        f"{searched}"
    )


def _load_library() -> ctypes.CDLL:
    library_path = _find_library()

    try:
        return ctypes.CDLL(
            str(library_path)
        )
    except OSError as exc:
        raise DENTError(
            f"Failed to load DENT API library:\n"
            f"{library_path}\n\n"
            f"{exc}"
        ) from exc


_lib = _load_library()


# ============================================================
# Native function declarations
# ============================================================

_lib.dent_solve_file_json.argtypes = [
    ctypes.c_char_p,
]

_lib.dent_solve_file_json.restype = (
    ctypes.c_void_p
)


_lib.dent_model_create.argtypes = [
    ctypes.c_int,
]

_lib.dent_model_create.restype = (
    ctypes.c_void_p
)


_lib.dent_model_create_from_file.argtypes = [
    ctypes.c_char_p,
]

_lib.dent_model_create_from_file.restype = (
    ctypes.c_void_p
)


_lib.dent_model_destroy.argtypes = [
    ctypes.c_void_p,
]

_lib.dent_model_destroy.restype = None


_lib.dent_model_add_variable.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_double,
    ctypes.c_double,
    ctypes.c_int,
]

_lib.dent_model_add_variable.restype = ctypes.c_int


_lib.dent_model_add_constraint.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_int,
    ctypes.c_double,
]

_lib.dent_model_add_constraint.restype = ctypes.c_int


_lib.dent_model_set_objective_coefficient.argtypes = [
    ctypes.c_void_p,
    ctypes.c_int,
    ctypes.c_double,
]

_lib.dent_model_set_objective_coefficient.restype = (
    ctypes.c_int
)


_lib.dent_model_set_constraint_coefficient.argtypes = [
    ctypes.c_void_p,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_double,
]

_lib.dent_model_set_constraint_coefficient.restype = (
    ctypes.c_int
)


_lib.dent_model_set_quadratic_coefficient.argtypes = [
    ctypes.c_void_p,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_double,
]

_lib.dent_model_set_quadratic_coefficient.restype = (
    ctypes.c_int
)


_lib.dent_model_set_variable_type.argtypes = [
    ctypes.c_void_p,
    ctypes.c_int,
    ctypes.c_int,
]

_lib.dent_model_set_variable_type.restype = (
    ctypes.c_int
)


_lib.dent_model_set_variable_bounds.argtypes = [
    ctypes.c_void_p,
    ctypes.c_int,
    ctypes.c_double,
    ctypes.c_double,
]

_lib.dent_model_set_variable_bounds.restype = (
    ctypes.c_int
)


_lib.dent_model_solve_json.argtypes = [
    ctypes.c_void_p,
]

_lib.dent_model_solve_json.restype = (
    ctypes.c_void_p
)


_lib.dent_last_error.argtypes = []

_lib.dent_last_error.restype = (
    ctypes.c_char_p
)


_lib.dent_free_string.argtypes = [
    ctypes.c_void_p,
]

_lib.dent_free_string.restype = None


# ============================================================
# Constants
# ============================================================

_VARIABLE_TYPES = {
    "continuous": 0,
    "integer": 1,
    "binary": 2,
}


_CONSTRAINT_SENSES = {
    "<=": 0,
    "le": 0,
    "less_equal": 0,

    "=": 1,
    "eq": 1,
    "equal": 1,

    ">=": 2,
    "ge": 2,
    "greater_equal": 2,
}


# ============================================================
# Native error handling
# ============================================================

def _last_error() -> str:
    raw = _lib.dent_last_error()

    if not raw:
        return "Unknown DENT native API error."

    return raw.decode(
        "utf-8",
        errors="replace",
    )


def _raise_last_error(
    operation: str,
) -> None:
    raise DENTError(
        f"{operation} failed: {_last_error()}"
    )


# ============================================================
# JSON result handling
# ============================================================

def _decode_result(
    raw_pointer: int | None,
) -> SolveResult:
    if not raw_pointer:
        _raise_last_error(
            "DENT solve"
        )

    try:
        raw_bytes = ctypes.string_at(
            raw_pointer
        )

        raw_json = raw_bytes.decode(
            "utf-8"
        )

    finally:
        _lib.dent_free_string(
            raw_pointer
        )

    try:
        result = json.loads(
            raw_json
        )
    except json.JSONDecodeError as exc:
        raise DENTError(
            "DENT native API returned invalid JSON:\n"
            f"{raw_json}"
        ) from exc

    if not isinstance(result, dict):
        raise DENTError(
            "DENT native API returned a JSON value "
            "that is not an object."
        )

    return SolveResult(result)


# ============================================================
# File-based API
# ============================================================

def solve(
    model_path: str | os.PathLike[str],
) -> SolveResult:
    """
    Solve a DENT .dent model file.

    Example
    -------
    result = dent.solve("model.dent")

    print(result.status)
    print(result.objective)
    """

    path = Path(
        model_path
    ).expanduser().resolve()

    if not path.is_file():
        raise FileNotFoundError(
            f"DENT model file does not exist: {path}"
        )

    encoded_path = os.fsencode(
        str(path)
    )

    raw_pointer = _lib.dent_solve_file_json(
        encoded_path
    )

    return _decode_result(
        raw_pointer
    )


# ============================================================
# Programmatic model API
# ============================================================

class Model:
    """
    Programmatic DENT optimization model.

    The model is backed directly by the native DENT
    Problem representation.

    Examples
    --------

    model = dent.Model("maximize")

    x = model.add_variable(
        "x",
        lower_bound=0,
    )

    y = model.add_variable(
        "y",
        lower_bound=0,
    )

    resource = model.add_constraint(
        "resource",
        "<=",
        40,
    )

    model.set_objective(x, 3)
    model.set_objective(y, 2)

    model.set_coefficient(
        resource,
        x,
        2,
    )

    model.set_coefficient(
        resource,
        y,
        1,
    )

    result = model.solve()
    """

    def __init__(
        self,
        sense: str = "maximize",
    ) -> None:

        normalized_sense = (
            str(sense)
            .strip()
            .lower()
        )

        if normalized_sense in {
            "maximize",
            "max",
            "maximum",
        }:
            maximize = 1

        elif normalized_sense in {
            "minimize",
            "min",
            "minimum",
        }:
            maximize = 0

        else:
            raise ValueError(
                "Model sense must be "
                "'maximize' or 'minimize'."
            )

        handle = _lib.dent_model_create(
            maximize
        )

        if not handle:
            _raise_last_error(
                "DENT model creation"
            )

        self._handle = handle

        self._variables: dict[str, int] = {}
        self._constraints: dict[str, int] = {}

    @classmethod
    def from_file(
        cls,
        model_path: str | os.PathLike[str],
    ) -> "Model":
        """
        Create a programmatic model from an existing
        .dent model file.

        The returned model can then be modified through
        the programmatic API before solving.
        """

        path = Path(
            model_path
        ).expanduser().resolve()

        if not path.is_file():
            raise FileNotFoundError(
                f"DENT model file does not exist: {path}"
            )

        handle = _lib.dent_model_create_from_file(
            os.fsencode(
                str(path)
            )
        )

        if not handle:
            _raise_last_error(
                "DENT model creation from file"
            )

        instance = cls.__new__(
            cls
        )

        instance._handle = handle

        instance._variables = {}
        instance._constraints = {}

        return instance

    def _ensure_open(self) -> None:
        if not self._handle:
            raise DENTError(
                "DENT model has already been closed."
            )

    def _variable_index(
        self,
        variable: int | str,
    ) -> int:
        if isinstance(variable, int):
            return variable

        try:
            return self._variables[
                variable
            ]
        except KeyError as exc:
            raise KeyError(
                f"Unknown DENT variable: {variable}"
            ) from exc

    def _constraint_index(
        self,
        constraint: int | str,
    ) -> int:
        if isinstance(constraint, int):
            return constraint

        try:
            return self._constraints[
                constraint
            ]
        except KeyError as exc:
            raise KeyError(
                f"Unknown DENT constraint: {constraint}"
            ) from exc

    def add_variable(
        self,
        name: str,
        lower_bound: float = 0.0,
        upper_bound: float = 0.0,
        type: str = "continuous",
    ) -> int:
        """
        Add a variable.

        Returns its zero-based native index.

        upper_bound=0 means +infinity, matching DENT's
        current Problem representation.
        """

        self._ensure_open()

        if name in self._variables:
            raise ValueError(
                f"DENT variable already exists: {name}"
            )

        normalized_type = (
            str(type)
            .strip()
            .lower()
        )

        if normalized_type not in _VARIABLE_TYPES:
            raise ValueError(
                "Variable type must be "
                "'continuous', 'integer', or 'binary'."
            )

        index = _lib.dent_model_add_variable(
            self._handle,
            os.fsencode(name),
            float(lower_bound),
            float(upper_bound),
            _VARIABLE_TYPES[
                normalized_type
            ],
        )

        if index < 0:
            _raise_last_error(
                "Adding DENT variable"
            )

        self._variables[name] = index

        return index

    def add_constraint(
        self,
        name: str,
        sense: str,
        rhs: float,
    ) -> int:
        """
        Add a linear constraint.

        sense may be:

            "<="
            "="
            ">="

        Returns its zero-based native index.
        """

        self._ensure_open()

        if name in self._constraints:
            raise ValueError(
                f"DENT constraint already exists: {name}"
            )

        normalized_sense = (
            str(sense)
            .strip()
            .lower()
        )

        if normalized_sense not in _CONSTRAINT_SENSES:
            raise ValueError(
                "Constraint sense must be "
                "'<=', '=', or '>='."
            )

        index = _lib.dent_model_add_constraint(
            self._handle,
            os.fsencode(name),
            _CONSTRAINT_SENSES[
                normalized_sense
            ],
            float(rhs),
        )

        if index < 0:
            _raise_last_error(
                "Adding DENT constraint"
            )

        self._constraints[name] = index

        return index

    def set_objective(
        self,
        variable: int | str,
        coefficient: float,
    ) -> None:
        """
        Set a linear objective coefficient.
        """

        self._ensure_open()

        variable_index = self._variable_index(
            variable
        )

        result = (
            _lib.dent_model_set_objective_coefficient(
                self._handle,
                variable_index,
                float(coefficient),
            )
        )

        if result != 0:
            _raise_last_error(
                "Setting DENT objective coefficient"
            )

    def set_coefficient(
        self,
        constraint: int | str,
        variable: int | str,
        coefficient: float,
    ) -> None:
        """
        Set a linear constraint coefficient.
        """

        self._ensure_open()

        constraint_index = self._constraint_index(
            constraint
        )

        variable_index = self._variable_index(
            variable
        )

        result = (
            _lib.dent_model_set_constraint_coefficient(
                self._handle,
                constraint_index,
                variable_index,
                float(coefficient),
            )
        )

        if result != 0:
            _raise_last_error(
                "Setting DENT constraint coefficient"
            )

    def set_quadratic_coefficient(
        self,
        row_variable: int | str,
        column_variable: int | str,
        coefficient: float,
    ) -> None:
        """
        Set a quadratic objective coefficient.

        DENT represents the objective as:

            1/2 x^T Q x + c^T x
        """

        self._ensure_open()

        row_index = self._variable_index(
            row_variable
        )

        column_index = self._variable_index(
            column_variable
        )

        result = (
            _lib.dent_model_set_quadratic_coefficient(
                self._handle,
                row_index,
                column_index,
                float(coefficient),
            )
        )

        if result != 0:
            _raise_last_error(
                "Setting DENT quadratic coefficient"
            )

    def set_variable_type(
        self,
        variable: int | str,
        type: str,
    ) -> None:
        """
        Change a variable type.
        """

        self._ensure_open()

        normalized_type = (
            str(type)
            .strip()
            .lower()
        )

        if normalized_type not in _VARIABLE_TYPES:
            raise ValueError(
                "Variable type must be "
                "'continuous', 'integer', or 'binary'."
            )

        variable_index = self._variable_index(
            variable
        )

        result = (
            _lib.dent_model_set_variable_type(
                self._handle,
                variable_index,
                _VARIABLE_TYPES[
                    normalized_type
                ],
            )
        )

        if result != 0:
            _raise_last_error(
                "Setting DENT variable type"
            )

    def set_bounds(
        self,
        variable: int | str,
        lower_bound: float,
        upper_bound: float = 0.0,
    ) -> None:
        """
        Change variable bounds.

        upper_bound=0 means +infinity.
        """

        self._ensure_open()

        variable_index = self._variable_index(
            variable
        )

        result = (
            _lib.dent_model_set_variable_bounds(
                self._handle,
                variable_index,
                float(lower_bound),
                float(upper_bound),
            )
        )

        if result != 0:
            _raise_last_error(
                "Setting DENT variable bounds"
            )

    def solve(self) -> SolveResult:
        """
        Solve the current model.
        """

        self._ensure_open()

        raw_pointer = (
            _lib.dent_model_solve_json(
                self._handle
            )
        )

        return _decode_result(
            raw_pointer
        )

    def close(self) -> None:
        """
        Explicitly release the native model.
        """

        if self._handle:
            _lib.dent_model_destroy(
                self._handle
            )

            self._handle = None

    def __enter__(self) -> "Model":
        self._ensure_open()
        return self

    def __exit__(
        self,
        exc_type: Any,
        exc_value: Any,
        traceback: Any,
    ) -> None:
        self.close()

    def __del__(self) -> None:
        handle = getattr(
            self,
            "_handle",
            None,
        )

        if handle:
            try:
                _lib.dent_model_destroy(
                    handle
                )
            except Exception:
                pass

            self._handle = None


__all__ = [
    "DENTError",
    "Model",
    "SolveResult",
    "solve",
]
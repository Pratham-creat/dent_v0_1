from __future__ import annotations

import ctypes
import json
import os
import platform
from pathlib import Path
from typing import Any


class DENTError(RuntimeError):
    """Raised when the DENT native API cannot be loaded or called."""


def _library_candidates() -> list[Path]:
    package_dir = Path(__file__).resolve().parent
    project_dir = package_dir.parent.parent

    candidates: list[Path] = []

    if os.name == "nt":
        names = [
            "dent_api.dll",
            "Release/dent_api.dll",
            "Debug/dent_api.dll",
        ]
    elif platform.system() == "Darwin":
        names = [
            "libdent_api.dylib",
        ]
    else:
        names = [
            "libdent_api.so",
        ]

    for name in names:
        candidates.append(project_dir / "build" / name)

    for name in names:
        candidates.append(project_dir / "build" / "Release" / name)

    for name in names:
        candidates.append(project_dir / "build" / "Debug" / name)

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


_lib.dent_solve_file_json.argtypes = [
    ctypes.c_char_p,
]

_lib.dent_solve_file_json.restype = (
    ctypes.c_void_p
)


_lib.dent_free_string.argtypes = [
    ctypes.c_void_p,
]

_lib.dent_free_string.restype = None


def solve(
    model_path: str | os.PathLike[str],
) -> dict[str, Any]:
    """
    Solve a DENT .dent model file.

    Parameters
    ----------
    model_path:
        Path to a DENT model file.

    Returns
    -------
    dict
        JSON result returned by the DENT native solver.
    """

    path = Path(model_path).expanduser().resolve()

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

    if not raw_pointer:
        raise DENTError(
            "DENT native API returned a null result."
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

    return result


__all__ = [
    "DENTError",
    "solve",
]
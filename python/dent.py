"""Thin ctypes Python API for the native DENT C API."""
from __future__ import annotations
import ctypes
import json
import os
from pathlib import Path

class DentError(RuntimeError):
    pass

class Model:
    def __init__(self, library=None, maximize=False):
        if library is None:
            library = os.environ.get("DENT_LIBRARY", "dent_api.dll" if os.name=="nt" else "libdent_api.so")
        self._lib=ctypes.CDLL(str(Path(library)))
        L=self._lib
        L.dent_model_create.argtypes=[ctypes.c_int]; L.dent_model_create.restype=ctypes.c_void_p
        L.dent_model_destroy.argtypes=[ctypes.c_void_p]
        L.dent_model_add_variable.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_double,ctypes.c_double,ctypes.c_int]
        L.dent_model_add_variable.restype=ctypes.c_int
        L.dent_model_add_constraint.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_int,ctypes.c_double]
        L.dent_model_add_constraint.restype=ctypes.c_int
        L.dent_model_set_objective_coefficient.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_double]
        L.dent_model_set_objective_coefficient.restype=ctypes.c_int
        L.dent_model_set_constraint_coefficient.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_double]
        L.dent_model_set_constraint_coefficient.restype=ctypes.c_int
        L.dent_model_solve_json.restype=ctypes.c_void_p
        L.dent_free_string.argtypes=[ctypes.c_void_p]
        L.dent_last_error.restype=ctypes.c_char_p
        self._model=L.dent_model_create(int(maximize))
        if not self._model: raise DentError(self.error())
    def error(self):
        return self._lib.dent_last_error().decode()
    def add_variable(self,name,lower=0.0,upper=0.0,kind=0):
        i=self._lib.dent_model_add_variable(self._model,name.encode(),lower,upper,kind)
        if i<0: raise DentError(self.error())
        return i
    def add_constraint(self,name,sense,rhs):
        i=self._lib.dent_model_add_constraint(self._model,name.encode(),sense,rhs)
        if i<0: raise DentError(self.error())
        return i
    def set_objective(self,var,coefficient):
        if self._lib.dent_model_set_objective_coefficient(self._model,var,coefficient)!=0: raise DentError(self.error())
    def set_coefficient(self,row,var,coefficient):
        if self._lib.dent_model_set_constraint_coefficient(self._model,row,var,coefficient)!=0: raise DentError(self.error())
    def solve(self):
        ptr=self._lib.dent_model_solve_json(self._model)
        if not ptr: raise DentError(self.error())
        try: return json.loads(ctypes.string_at(ptr).decode())
        finally: self._lib.dent_free_string(ptr)
    def close(self):
        if self._model:
            self._lib.dent_model_destroy(self._model); self._model=None
    def __del__(self):
        try:self.close()
        except Exception:pass

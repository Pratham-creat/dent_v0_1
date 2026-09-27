from pathlib import Path
import ctypes
import json
import os
import platform
import tempfile
from typing import Literal

from fastapi import FastAPI, File, HTTPException, UploadFile
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field

ROOT = Path(__file__).resolve().parents[1]

class Variable(BaseModel):
    name: str
    lower_bound: float = 0.0
    upper_bound: float = 0.0
    type: Literal["continuous", "integer", "binary"] = "continuous"
    objective_coefficient: float = 0.0

class Constraint(BaseModel):
    name: str
    sense: int = Field(..., ge=0, le=2)
    rhs: float
    coefficients: dict[str, float] = {}

class QuadraticTerm(BaseModel):
    row: str
    column: str
    coefficient: float

class SolveRequest(BaseModel):
    objective: Literal["minimize", "maximize"] = "minimize"
    variables: list[Variable]
    constraints: list[Constraint] = []
    quadratic_terms: list[QuadraticTerm] = []

app = FastAPI(title="DENT Optimization Engine API", version="0.1.0")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_credentials=True,
                   allow_methods=["*"], allow_headers=["*"])

def candidates():
    override = os.getenv("DENT_API_LIBRARY")
    if override:
        return [Path(override)]
    system = platform.system().lower()
    name = "dent_api.dll" if "windows" in system else ("libdent_api.dylib" if "darwin" in system else "libdent_api.so")
    return [ROOT / p / name for p in ("build", "build/Release", "build/Debug")]

class DentAPI:
    def __init__(self):
        self.path = next((p for p in candidates() if p.exists()), None)
        if not self.path:
            raise FileNotFoundError("DENT native API library not found. Build dent_api or set DENT_API_LIBRARY.")
        self.lib = ctypes.CDLL(str(self.path))
        self._bind()

    def _bind(self):
        L = self.lib
        L.dent_model_create.argtypes=[ctypes.c_int]; L.dent_model_create.restype=ctypes.c_void_p
        L.dent_model_destroy.argtypes=[ctypes.c_void_p]
        L.dent_model_add_variable.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_double,ctypes.c_double,ctypes.c_int]; L.dent_model_add_variable.restype=ctypes.c_int
        L.dent_model_add_constraint.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_int,ctypes.c_double]; L.dent_model_add_constraint.restype=ctypes.c_int
        L.dent_model_set_objective_coefficient.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_double]; L.dent_model_set_objective_coefficient.restype=ctypes.c_int
        L.dent_model_set_constraint_coefficient.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_double]; L.dent_model_set_constraint_coefficient.restype=ctypes.c_int
        L.dent_model_set_quadratic_coefficient.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_double]; L.dent_model_set_quadratic_coefficient.restype=ctypes.c_int
        L.dent_model_set_variable_type.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_int]; L.dent_model_set_variable_type.restype=ctypes.c_int
        L.dent_model_solve_json.argtypes=[ctypes.c_void_p]; L.dent_model_solve_json.restype=ctypes.c_void_p
        L.dent_solve_file_json.argtypes=[ctypes.c_char_p]; L.dent_solve_file_json.restype=ctypes.c_void_p
        L.dent_last_error.restype=ctypes.c_char_p
        L.dent_free_string.argtypes=[ctypes.c_void_p]

    def error(self):
        raw=self.lib.dent_last_error()
        return raw.decode("utf-8","replace") if raw else "Unknown DENT error"

    def decode(self, ptr):
        if not ptr: raise RuntimeError(self.error())
        try:
            raw=ctypes.cast(ptr,ctypes.c_char_p).value
            return json.loads(raw.decode("utf-8"))
        finally:
            self.lib.dent_free_string(ptr)

    def solve(self, req):
        model=self.lib.dent_model_create(1 if req.objective=="maximize" else 0)
        if not model: raise RuntimeError(self.error())
        try:
            ids={}; types={"continuous":0,"integer":1,"binary":2}
            for i,v in enumerate(req.variables):
                if self.lib.dent_model_add_variable(model,v.name.encode(),v.lower_bound,v.upper_bound,types[v.type]) < 0: raise RuntimeError(self.error())
                ids[v.name]=i
                if self.lib.dent_model_set_objective_coefficient(model,i,v.objective_coefficient): raise RuntimeError(self.error())
            for j,c in enumerate(req.constraints):
                if self.lib.dent_model_add_constraint(model,c.name.encode(),c.sense,c.rhs) < 0: raise RuntimeError(self.error())
                for name,coef in c.coefficients.items():
                    if name not in ids: raise HTTPException(400,f"Unknown variable: {name}")
                    if self.lib.dent_model_set_constraint_coefficient(model,j,ids[name],coef): raise RuntimeError(self.error())
            for q in req.quadratic_terms:
                if q.row not in ids or q.column not in ids: raise HTTPException(400,"Unknown variable in quadratic term")
                if self.lib.dent_model_set_quadratic_coefficient(model,ids[q.row],ids[q.column],q.coefficient): raise RuntimeError(self.error())
            return self.decode(self.lib.dent_model_solve_json(model))
        finally:
            self.lib.dent_model_destroy(model)

    def solve_file(self,data):
        path=None
        try:
            with tempfile.NamedTemporaryFile(delete=False,suffix=".dent") as f:
                f.write(data); path=f.name
            return self.decode(self.lib.dent_solve_file_json(path.encode()))
        finally:
            if path: Path(path).unlink(missing_ok=True)

def native():
    try: return DentAPI()
    except Exception as e: raise HTTPException(503,str(e))

@app.get("/health")
def health():
    try:
        api=DentAPI()
        return {"status":"ok","native_api":True,"library":str(api.path)}
    except Exception as e:
        return {"status":"degraded","native_api":False,"error":str(e)}

@app.get("/api/v1/capabilities")
def capabilities():
    return {"engine":"DENT Optimization Engine","problem_classes":["LP","MILP","QP"],
            "solvers":["PrimalSimplex","InteriorPoint","PDHG","PDLP","QP","MILP"],
            "transport":"REST/JSON"}

@app.post("/api/v1/solve")
def solve(req:SolveRequest):
    return native().solve(req)

@app.post("/api/v1/solve-file")
async def solve_file(file:UploadFile=File(...)):
    if not file.filename or Path(file.filename).suffix.lower()!=".dent":
        raise HTTPException(400,"Only .dent model files are supported.")
    return native().solve_file(await file.read())

if __name__=="__main__":
    import uvicorn
    uvicorn.run(app,host="127.0.0.1",port=8000)

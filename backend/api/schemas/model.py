from typing import Any, Literal

from pydantic import BaseModel, Field

from api.schemas.errors import ErrorResponse


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


class SolverOptions(BaseModel):
    method: Literal[
        "auto",
        "primal_simplex",
        "interior_point",
        "pdhg",
        "pdlp",
        "qp",
        "milp",
    ] = "auto"
    tolerance: float = Field(default=0.0, ge=0.0)
    max_iterations: int = Field(default=0, ge=0)


class SolveRequest(BaseModel):
    objective: Literal["minimize", "maximize"] = "minimize"
    variables: list[Variable]
    constraints: list[Constraint] = []
    quadratic_terms: list[QuadraticTerm] = []
    solver: SolverOptions = Field(default_factory=SolverOptions)


class VariableResult(BaseModel):
    name: str
    value: float


class Fingerprint(BaseModel):
    variables: int
    constraints: int
    nonzeros: int
    density: float
    mixed_integer: bool
    quadratic: bool


class SolveResult(BaseModel):
    status: str
    objective: float
    solver: str
    iterations: int
    message: str
    variables: list[VariableResult]
    fingerprint: Fingerprint
    solve_time_ms: float | None = None
    configuration: SolverOptions | None = None


class RunResponse(BaseModel):
    id: str
    created_at: str
    source_type: str
    source_name: str | None = None
    result: SolveResult

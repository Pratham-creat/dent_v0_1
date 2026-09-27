from typing import Literal

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


class SolveRequest(BaseModel):
    objective: Literal["minimize", "maximize"] = "minimize"
    variables: list[Variable]
    constraints: list[Constraint] = []
    quadratic_terms: list[QuadraticTerm] = []

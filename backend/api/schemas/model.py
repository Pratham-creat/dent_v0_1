from typing import Literal

from pydantic import BaseModel, Field


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
    coefficients: dict[str, float] = Field(default_factory=dict)


class QuadraticTerm(BaseModel):
    row: str
    column: str
    coefficient: float


class SolveRequest(BaseModel):
    objective: Literal["minimize", "maximize"] = "minimize"
    variables: list[Variable]
    constraints: list[Constraint] = Field(default_factory=list)
    quadratic_terms: list[QuadraticTerm] = Field(default_factory=list)

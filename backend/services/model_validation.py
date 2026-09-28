from math import isfinite

from fastapi import HTTPException

from api.schemas.model import SolveRequest


def validate_solve_request(request: SolveRequest) -> None:
    """Validate model-level invariants before invoking the native solver."""
    if not request.variables:
        raise HTTPException(400, "At least one variable is required.")

    variable_names = [variable.name.strip() for variable in request.variables]
    if any(not name for name in variable_names):
        raise HTTPException(400, "Variable names must not be empty.")

    duplicates = sorted({name for name in variable_names if variable_names.count(name) > 1})
    if duplicates:
        raise HTTPException(400, f"Duplicate variable name(s): {', '.join(duplicates)}")

    variable_set = set(variable_names)

    for variable in request.variables:
        if not isfinite(variable.lower_bound):
            raise HTTPException(400, f"Variable '{variable.name}' has a non-finite lower bound.")
        if not isfinite(variable.upper_bound):
            raise HTTPException(400, f"Variable '{variable.name}' has a non-finite upper bound.")
        if not isfinite(variable.objective_coefficient):
            raise HTTPException(400, f"Variable '{variable.name}' has a non-finite objective coefficient.")

        # DENT uses upper_bound == 0 to represent +infinity.
        if variable.upper_bound != 0 and variable.upper_bound < variable.lower_bound:
            raise HTTPException(
                400,
                f"Variable '{variable.name}' has an upper bound smaller than its lower bound.",
            )

    constraint_names = [constraint.name.strip() for constraint in request.constraints]
    if any(not name for name in constraint_names):
        raise HTTPException(400, "Constraint names must not be empty.")

    duplicate_constraints = sorted(
        {name for name in constraint_names if constraint_names.count(name) > 1}
    )
    if duplicate_constraints:
        raise HTTPException(
            400,
            f"Duplicate constraint name(s): {', '.join(duplicate_constraints)}",
        )

    for constraint in request.constraints:
        if not isfinite(constraint.rhs):
            raise HTTPException(400, f"Constraint '{constraint.name}' has a non-finite RHS.")

        for name, coefficient in constraint.coefficients.items():
            if name not in variable_set:
                raise HTTPException(400, f"Unknown variable: {name}")
            if not isfinite(coefficient):
                raise HTTPException(
                    400,
                    f"Constraint '{constraint.name}' has a non-finite coefficient for '{name}'.",
                )

    for term in request.quadratic_terms:
        if term.row not in variable_set or term.column not in variable_set:
            raise HTTPException(400, "Unknown variable in quadratic term.")
        if not isfinite(term.coefficient):
            raise HTTPException(400, "Quadratic terms must have finite coefficients.")

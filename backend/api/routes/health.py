from fastapi import APIRouter

from services.solver_service import solver_service


router = APIRouter()


@router.get("/health")
def health():
    return solver_service.health()


@router.get("/api/v1/capabilities")
def capabilities():
    return {
        "engine": "DENT Optimization Engine",
        "problem_classes": ["LP", "MILP", "QP"],
        "solvers": [
            "PrimalSimplex",
            "InteriorPoint",
            "PDHG",
            "PDLP",
            "QP",
            "MILP",
        ],
        "transport": "REST/JSON",
    }

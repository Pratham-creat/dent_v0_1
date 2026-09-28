from fastapi import APIRouter

from services.capabilities_service import capabilities_service
from services.solver_service import solver_service


router = APIRouter()


@router.get("/health")
def health():
    return solver_service.health()


@router.get("/api/v1/capabilities")
def capabilities():
    return capabilities_service.get()

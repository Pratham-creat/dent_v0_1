from native.dent_api import DentAPI


ENGINE_VERSION = "0.11.0"
API_VERSION = "0.11.0"


class CapabilitiesService:
    def get(self):
        native_api = self._native_status()

        return {
            "engine": {
                "name": "DENT Optimization Engine",
                "version": ENGINE_VERSION,
                "api_version": API_VERSION,
            },
            "problem_classes": ["LP", "MILP", "QP"],
            "solvers": [
                {"id": "PrimalSimplex", "category": "LP"},
                {"id": "DualSimplex", "category": "LP"},
                {"id": "InteriorPoint", "category": "LP"},
                {"id": "PDHG", "category": "LP"},
                {"id": "PDLP", "category": "LP"},
                {"id": "QP", "category": "QP"},
                {"id": "MILP", "category": "MILP"},
            ],
            "model": {
                "objective_senses": ["minimize", "maximize"],
                "variable_types": ["continuous", "integer", "binary"],
                "constraint_senses": ["<=", "=", ">="],
                "quadratic_terms": True,
            },
            "input": {
                "formats": ["JSON", ".dent"],
                "transport": "REST/JSON",
            },
            "output": {
                "format": "JSON",
                "includes": [
                    "status",
                    "objective",
                    "solver",
                    "iterations",
                    "message",
                    "variables",
                    "fingerprint",
                ],
            },
            "features": {
                "presolve": True,
                "scaling": True,
                "adaptive_dispatch": True,
                "solution_validation": True,
                "warm_start": True,
                "milp_branch_and_cut": True,
                "run_history": True,
            },
            "persistence": {
                "enabled": True,
                "backend": "SQLite",
            },
            "native_api": native_api,
        }

    @staticmethod
    def _native_status():
        try:
            api = DentAPI()
            return {
                "available": True,
                "library": str(api.path),
            }
        except Exception as exc:
            return {
                "available": False,
                "error": str(exc),
            }


capabilities_service = CapabilitiesService()

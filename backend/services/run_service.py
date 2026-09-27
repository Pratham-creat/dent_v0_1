from datetime import datetime, timezone
from uuid import uuid4

from storage import run_repository


class RunService:
    def create(self, result, source_type, source_name=None):
        run = {
            "id": str(uuid4()),
            "created_at": datetime.now(timezone.utc).isoformat(),
            "source_type": source_type,
            "source_name": source_name,
            "result": result,
        }
        return run_repository.save(run)

    def list(self, limit=50, offset=0):
        return run_repository.list(limit=limit, offset=offset)

    def get(self, run_id):
        return run_repository.get(run_id)

    def delete(self, run_id):
        return run_repository.delete(run_id)

    def clear(self):
        return run_repository.clear()


run_service = RunService()

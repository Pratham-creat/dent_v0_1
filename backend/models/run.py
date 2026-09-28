from dataclasses import dataclass
from typing import Any


@dataclass(frozen=True)
class RunRecord:
    id: str
    created_at: str
    source_type: str
    source_name: str | None
    result: dict[str, Any]

from typing import Any

from pydantic import BaseModel


class ErrorResponse(BaseModel):
    detail: str
    code: str
    status: int
    errors: list[dict[str, Any]] | None = None

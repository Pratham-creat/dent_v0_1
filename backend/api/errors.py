from typing import Any

from fastapi import HTTPException, Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse


def error_response(
    status_code: int,
    code: str,
    detail: str,
    errors: list[dict[str, Any]] | None = None,
):
    content = {
        "detail": detail,
        "code": code,
        "status": status_code,
    }
    if errors:
        content["errors"] = errors

    return JSONResponse(status_code=status_code, content=content)


async def http_exception_handler(request: Request, exc: HTTPException):
    code = exc.headers.get("X-DENT-Error-Code") if exc.headers else None
    return error_response(
        exc.status_code,
        code or _default_code(exc.status_code),
        str(exc.detail),
    )


async def validation_exception_handler(request: Request, exc: RequestValidationError):
    return error_response(
        422,
        "REQUEST_VALIDATION_ERROR",
        "Request validation failed.",
        errors=exc.errors(),
    )


async def unhandled_exception_handler(request: Request, exc: Exception):
    return error_response(
        500,
        "INTERNAL_ERROR",
        "An unexpected internal error occurred.",
    )


def _default_code(status_code: int):
    return {
        400: "BAD_REQUEST",
        404: "NOT_FOUND",
        409: "CONFLICT",
        422: "REQUEST_VALIDATION_ERROR",
        500: "INTERNAL_ERROR",
        503: "NATIVE_API_UNAVAILABLE",
    }.get(status_code, "API_ERROR")

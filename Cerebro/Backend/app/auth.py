"""Cookie-session authentication and tenant-scope middleware."""

from __future__ import annotations

import os
import time
from typing import Any

from fastapi import Request
from starlette.responses import JSONResponse

from .services.auth import AuthSession, InvalidSession
from .services.tenancy import reset_current_scope, set_current_scope


SESSION_COOKIE = "kira_session"
CSRF_COOKIE = "kira_csrf"
CSRF_HEADER = "x-kira-csrf"
SAFE_METHODS = {"GET", "HEAD", "OPTIONS", "TRACE"}


def cookie_secure(request: Request) -> bool:
    configured = os.getenv("KIRA_COOKIE_SECURE", "").strip().casefold()
    if configured in {"1", "true", "yes", "on"}:
        return True
    if configured in {"0", "false", "no", "off"}:
        return False
    return request.url.scheme == "https"


def set_session_cookies(
    response: Any,
    request: Request,
    raw_token: str,
    raw_csrf: str,
    expires_at: int,
) -> None:
    secure = cookie_secure(request)
    max_age = max(1, int(expires_at - time.time()))
    response.set_cookie(
        SESSION_COOKIE,
        raw_token,
        max_age=max_age,
        httponly=True,
        secure=secure,
        samesite="lax",
        path="/",
    )
    # Esta cookie es deliberadamente legible por el SPA: el backend lo
    # compara con el hash guardado en la sesión para el header CSRF.
    response.set_cookie(
        CSRF_COOKIE,
        raw_csrf,
        max_age=max_age,
        httponly=False,
        secure=secure,
        samesite="lax",
        path="/",
    )
    response.headers["Cache-Control"] = "no-store"


def clear_session_cookies(response: Any, request: Request) -> None:
    secure = cookie_secure(request)
    response.delete_cookie(SESSION_COOKIE, path="/", secure=secure, httponly=True, samesite="lax")
    response.delete_cookie(CSRF_COOKIE, path="/", secure=secure, httponly=False, samesite="lax")
    response.headers["Cache-Control"] = "no-store"


def _json_error(status: int, detail: str) -> JSONResponse:
    response = JSONResponse({"detail": detail}, status_code=status)
    response.headers["Cache-Control"] = "no-store"
    return response


class TenantAuthMiddleware:
    """Resolve the opaque session and install the user's data scope.

    This is a pure ASGI middleware on purpose: the context remains active while
    a StreamingResponse body is consumed, which is required for SSE memory/RAG
    work and its background tasks.
    """

    def __init__(self, app: Any, backend_context: Any) -> None:
        self.app = app
        self.auth_store = backend_context.auth_store
        self.tenant_manager = backend_context.tenant_manager
        self.auth_required = bool(backend_context.auth_required)

    async def __call__(self, scope: dict, receive: Any, send: Any) -> None:
        if scope.get("type") != "http":
            await self.app(scope, receive, send)
            return

        path = str(scope.get("path", ""))
        method = str(scope.get("method", "GET")).upper()
        is_auth_route = path == "/api/auth" or path.startswith("/api/auth/")
        is_api = path.startswith("/api/")
        is_protected = is_api or path.startswith("/grabaciones/")
        if not is_protected or method == "OPTIONS":
            await self.app(scope, receive, send)
            return

        request = Request(scope, receive=receive)
        raw_token = request.cookies.get(SESSION_COOKIE)
        try:
            session = self.auth_store.get_session(raw_token)
        except InvalidSession:
            session = None

        # Registration/login are the only routes that must work without a
        # session.  The route itself handles its own CSRF/session rotation.
        if session is None and self.auth_required and not (
            path in {"/api/auth/register", "/api/auth/login", "/api/auth/me"}
        ):
            response = _json_error(401, "necesitás iniciar sesión")
            await response(scope, receive, send)
            return

        if session is None:
            await self.app(scope, receive, send)
            return

        tenant = self.tenant_manager.scope_for(session.user.id, session.user.username)
        scope.setdefault("state", {})["user"] = session.user
        scope["state"]["auth_session"] = session
        scope["state"]["tenant"] = tenant

        # Auth routes handle their own login/logout mechanics.  Every other
        # unsafe cookie-authenticated request needs the session-bound token.
        if method not in SAFE_METHODS and not is_auth_route:
            csrf_cookie = request.cookies.get(CSRF_COOKIE, "")
            csrf_header = request.headers.get(CSRF_HEADER, "")
            if not self.auth_store.check_csrf(session, csrf_header) or csrf_cookie != csrf_header:
                response = _json_error(403, "token CSRF inválido")
                await response(scope, receive, send)
                return

        token = set_current_scope(tenant)
        try:
            await self.app(scope, receive, send)
        finally:
            reset_current_scope(token)


__all__ = [
    "CSRF_COOKIE",
    "CSRF_HEADER",
    "SESSION_COOKIE",
    "TenantAuthMiddleware",
    "clear_session_cookies",
    "cookie_secure",
    "set_session_cookies",
]

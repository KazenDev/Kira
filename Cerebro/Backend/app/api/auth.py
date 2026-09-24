"""Public account and session endpoints."""

from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException, Request, Response

from ..auth import (
    CSRF_COOKIE,
    CSRF_HEADER,
    SESSION_COOKIE,
    clear_session_cookies,
    set_session_cookies,
)
from ..dependencies import BackendContext, get_backend_context
from ..schemas import AuthLoginRequest, AuthRegisterRequest
from ..services.auth import InvalidCredentials, InvalidSession, UsernameTaken

router = APIRouter()


def _password_valida(password: object) -> str:
    value = str(password or "")
    if not 8 <= len(value) <= 128:
        raise HTTPException(400, "la contraseña debe tener entre 8 y 128 caracteres")
    return value


@router.post("/api/auth/register")
async def register(
    body: AuthRegisterRequest,
    request: Request,
    response: Response,
    services: BackendContext = Depends(get_backend_context),
) -> dict:
    password = _password_valida(body.password)
    was_empty = services.auth_store.count_users() == 0
    try:
        user = services.auth_store.register(str(body.username or ""), password)
    except UsernameTaken as exc:
        raise HTTPException(409, str(exc)) from exc
    except ValueError as exc:
        raise HTTPException(400, str(exc)) from exc

    imported = False
    if was_empty:
        try:
            imported = services.tenant_manager.claim_legacy_for_first_user(
                user.id,
                user.username,
            )
        except Exception as exc:
            # La cuenta ya existe; no se revierte el registro.
            print(f"[AUTH] no pude importar el snapshot legacy: {type(exc).__name__}: {exc}")

    old_token = request.cookies.get(SESSION_COOKIE)
    services.auth_store.delete_session(old_token)
    raw_token, raw_csrf, session = services.auth_store.create_session(user)
    set_session_cookies(response, request, raw_token, raw_csrf, session.expires_at)
    return {
        "ok": True,
        "user": user.public(),
        "csrf_token": raw_csrf,
        "legacy_imported": imported,
    }


@router.post("/api/auth/login")
async def login(
    body: AuthLoginRequest,
    request: Request,
    response: Response,
    services: BackendContext = Depends(get_backend_context),
) -> dict:
    try:
        user = services.auth_store.authenticate(
            str(body.username or ""),
            _password_valida(body.password),
        )
    except InvalidCredentials as exc:
        raise HTTPException(401, str(exc)) from exc

    old_token = request.cookies.get(SESSION_COOKIE)
    services.auth_store.delete_session(old_token)
    raw_token, raw_csrf, session = services.auth_store.create_session(user)
    set_session_cookies(response, request, raw_token, raw_csrf, session.expires_at)
    return {"ok": True, "user": user.public(), "csrf_token": raw_csrf}


@router.get("/api/auth/me")
async def me(
    request: Request,
    services: BackendContext = Depends(get_backend_context),
) -> dict:
    try:
        session = services.auth_store.get_session(request.cookies.get(SESSION_COOKIE))
    except InvalidSession:
        return {"ok": False, "user": None, "csrf_token": ""}
    csrf = request.cookies.get(CSRF_COOKIE, "")
    # The raw CSRF value is only returned when the browser still has the
    # session-bound cookie.  It is never stored in localStorage.
    if not services.auth_store.check_csrf(session, csrf):
        csrf = ""
    return {"ok": True, "user": session.user.public(), "csrf_token": csrf}


@router.post("/api/auth/logout")
async def logout(
    request: Request,
    response: Response,
    services: BackendContext = Depends(get_backend_context),
) -> dict:
    raw_token = request.cookies.get(SESSION_COOKIE)
    try:
        session = services.auth_store.get_session(raw_token)
    except InvalidSession:
        clear_session_cookies(response, request)
        return {"ok": True}

    csrf_cookie = request.cookies.get(CSRF_COOKIE, "")
    csrf_header = request.headers.get(CSRF_HEADER, "")
    if csrf_cookie != csrf_header or not services.auth_store.check_csrf(session, csrf_header):
        raise HTTPException(403, "token CSRF inválido")
    services.auth_store.delete_session(raw_token)
    clear_session_cookies(response, request)
    return {"ok": True}


__all__ = ["router"]

"""Permissive request schemas introduced during modularization.

Unknown fields remain accepted and values use ``Any`` on purpose: endpoints
retain the exact legacy normalization and status codes while OpenAPI gains a
real request schema. Stricter limits belong to the dedicated hardening phase.
"""

from __future__ import annotations

from typing import Any

from pydantic import BaseModel, ConfigDict, Field


class _PermissiveModel(BaseModel):
    model_config = ConfigDict(extra="allow")


class TituloRequest(_PermissiveModel):
    primer_mensaje: Any = ""
    primera_respuesta: Any = ""


class ChatRequest(_PermissiveModel):
    personaje: Any = "kira"
    mensaje: Any = ""
    historia: Any = Field(default_factory=list)
    sesion: Any = None
    imagen: Any = None
    foto: Any = None
    memoria_config: Any = Field(default_factory=dict)


class AuthRegisterRequest(_PermissiveModel):
    username: Any = ""
    password: Any = ""


class AuthLoginRequest(_PermissiveModel):
    username: Any = ""
    password: Any = ""


class MemoriaOlvidarRequest(_PermissiveModel):
    id: Any = ""
    motivo: Any = "olvidado desde Ajustes"


class MemoriaBorrarTodoRequest(_PermissiveModel):
    motivo: Any = "memoria borrada desde Ajustes"


class BridgeTokenRequest(_PermissiveModel):
    token: Any = ""


class BridgeErrorRequest(_PermissiveModel):
    mensaje: Any = ""


class EmocionRequest(_PermissiveModel):
    emotion: Any = "happy"


class ComandoRequest(_PermissiveModel):
    cmd: Any = ""


class GrabarRequest(_PermissiveModel):
    duracion_ms: Any = 3000

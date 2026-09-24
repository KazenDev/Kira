"""Pure validation and prompt-building helpers for camera images."""

from __future__ import annotations

from typing import Any

IMAGEN_MIMES = ("image/jpeg", "image/png", "image/gif", "image/webp")
IMAGEN_MAX_B64 = 12 * 1024 * 1024
AVISO_FOTO = (
    "El usuario te acaba de mandar una FOTO sacada con la camara de su celular. "
    "MIRALA de verdad y reacciona a lo que se ve en ella (quien o que aparece, "
    "el lugar, los colores; si hay texto escrito, leelo), con tu estilo y en "
    "primera persona. Esta PROHIBIDO decir que no podes ver imagenes o que no "
    "te llego nada: la foto esta ahi. Si algo no se distingue, decí lo que si ves."
)


def normalizar_foto(valor: Any) -> str | None:
    """Validate a data URL without ever allowing malformed input to escape."""

    if not valor:
        return None
    if isinstance(valor, dict):
        valor = valor.get("datos") or valor.get("dataUrl") or valor.get("url") or ""
    datos = str(valor).strip()
    if not datos.startswith("data:") or ";base64," not in datos:
        print("[FOTO] la imagen no es un data URL base64: se ignora")
        return None
    cabecera, _, b64 = datos.partition(";base64,")
    mime = cabecera[5:].split(";")[0].strip().lower()
    if mime not in IMAGEN_MIMES:
        print(f"[FOTO] formato no soportado ({mime or 'sin mime'}): se ignora")
        return None
    if len(b64) < 100:
        print("[FOTO] la imagen vino vacia: se ignora")
        return None
    if len(b64) > IMAGEN_MAX_B64:
        print(f"[FOTO] imagen demasiado grande ({len(b64) // 1024} KB): se ignora")
        return None
    print(f"[FOTO] foto aceptada: {mime}, {len(b64) // 1024} KB")
    return f"data:{mime};base64,{b64}"


def mensaje_con_foto(mensaje: str, foto: str | None) -> str | list[dict[str, Any]]:
    """Build the text-only or multimodal user content expected by the provider."""

    if not foto:
        return mensaje
    texto = mensaje.strip() or "Mira la foto que te mando."
    return [
        {"type": "text", "text": f"{texto}\n\n[FOTO] {AVISO_FOTO}"},
        {"type": "image_url", "image_url": {"url": foto, "detail": "high"}},
    ]

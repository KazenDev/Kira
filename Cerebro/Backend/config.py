import os

# ================================================================================
#  KIRA - Configuracion central del Backend
#  El cerebro que conecta: DeepSeek (IA) + Fish Audio (voz) + micro:bit (cara)
# ================================================================================

# ------------------- IA (DeepSeek platform, API OpenAI-compatible) ----------------
# Antes: nano-gpt (sk-nano-...). Se pasó a la plataforma oficial de DeepSeek:
# mismo formato (POST /chat/completions + Bearer), así que las únicas líneas que
# hay que tocar son estas tres.
DEEPSEEK_BASE_URL = "https://api.deepseek.com"   # el alias .../v1 también vale
# La clave se carga desde .env (run.sh lo carga antes de iniciar Uvicorn).
# No se mantiene un fallback con el secreto en el código.
DEEPSEEK_API_KEY = os.getenv("DEEPSEEK_API_KEY", "")
NANO_BASE_URL = "https://nano-gpt.com/api/v1"
NANO_API_KEY = os.getenv("NANO_API_KEY", "")

# PRINCIPAL + RESPALDO. Se prueban EN ORDEN: se agota el primero (sus reintentos
# con backoff) y recién ahí entra el siguiente. Motivo real: DeepSeek responde
# HTTP 402 cuando se acaba el saldo — en medio de una demo, el respaldo salva la
# función. El log avisa: "[IA] deepseek se agoto (HTTP 402) -> CAE AL RESPALDO".
# OJO: cada proveedor lleva SUS extras. El `thinking` es solo de DeepSeek (V4
# piensa por defecto: para una charla con voz son segundos de silencio antes de
# la primera palabra); nano-gpt rechazaría ese campo.
PROVEEDORES_IA: list[dict] = [
    {
        "nombre": "deepseek",
        "base_url": DEEPSEEK_BASE_URL,
        "api_key": DEEPSEEK_API_KEY,
        # "deepseek-flash" = DeepSeek-V4.1-Flash (el legacy "deepseek-v4-flash"
        # apunta al mismo modelo). El otro es "deepseek-v4-pro" (más caro/lento).
        "modelo": "deepseek-flash",
        "extra": {"thinking": {"type": "disabled"}},
    },
    {
        "nombre": "nano-gpt (respaldo)",
        "base_url": NANO_BASE_URL,
        "api_key": NANO_API_KEY,
        "modelo": "google/gemma-4-26b-a4b-uncensored",  # roleplay cero-corporativo
        "extra": {},
    },
]
JSON_MODE = {"type": "json_object"}
MAX_TOKENS = 300
# El modelo puede consumir tokens de razonamiento antes de `content`.
MIN_TOKENS_IA = int(os.getenv("KIRA_MIN_TOKENS_IA", "128"))
# compat: los 5 call sites siguen escribiendo config.MODELO_IA / config.IA_EXTRA.
# El modelo real lo fija quien atienda (ver _cuerpo_para en kira_server).
MODELO_IA = PROVEEDORES_IA[0]["modelo"]
IA_EXTRA = PROVEEDORES_IA[0]["extra"]

# ------------------- Voz (Fish Audio) -------------------
FISH_BASE_URL = "https://api.fish.audio"
FISH_API_KEY = os.getenv("FISH_API_KEY", "")
FISH_MODELO = "s2.1-pro-free"  # GRATIS (el modelo va como HEADER, no en el body)

# ------------------- Transcripcion (AssemblyAI) -------------------
# Se carga desde .env; no se guarda ninguna credencial en el código.
ASSEMBLYAI_API_KEY = os.getenv("ASSEMBLYAI_API_KEY", "")

# ------------------- Busqueda web (Exa) -------------------
# Tool de la IA: buscar_en_web usa la API de Exa (semantica, con citas).
EXA_API_KEY = os.getenv("EXA_API_KEY", "")
EXA_BASE_URL = "https://api.exa.ai"

# Tags de emocion para que la VOZ suene con la misma emocion que la cara
# (formato oficial de Fish Audio S2: [bracket] con lenguaje natural, 64+ emociones)
TTS_TAGS = {
    "happy": "[happy]",
    "sad": "[sad]",
    "angry": "[angry]",
    "surprised": "[surprised]",
    "neutral": "",
    "fastidio": "[frustrated]",
    "miedo": "[scared]",
    "cansado": "[tired]",
}

# ------------------- Emociones -> comando serial del micro:bit -------------------
EMOCION_SERIAL = {
    "happy": "HAPPY",
    "sad": "SAD",
    "angry": "ANGRY",
    "surprised": "SURPRISED",
    "neutral": "NEUTRAL",
    "fastidio": "FASTIDIO",
    "miedo": "MIEDO",
    "cansado": "CANSADO",
    "talk": "TALK",
    "loading": "LOADING",
    "stop": "STOP",
}

# ------------------- Temperatura por emocion -------------------
# DeepSeek V4 Flash recomienda temperature=1.0 para rol/charlando natural.
# Las emociones varian alrededor de 1.0 (nunca tan bajas que suene robotico):
#   - emociones vivas -> mas creatividad (chistes, exagerar)
#   - emociones secas -> un pelin mas abajo, pero SIEMPRE hablado natural
TEMPERATURAS = {
    "happy": 1.25,
    "sad": 0.85,
    "angry": 0.9,
    "surprised": 1.2,
    "neutral": 1.0,
    "fastidio": 0.8,
    "miedo": 0.85,
    "cansado": 0.8,
}
TEMP_INICIAL = 1.0   # arranca en neutral
RAMP_PASO = 0.1      # la temperatura sube/baja de a 0.1 por respuesta (transicion suave)

# ------------------- Personajes disponibles -------------------
# Quedó SOLO Kira (mujer): se retiró Kiro. Los endpoints validan contra esta
# lista (cualquier otro id -> 404) y el frontend solo muestra lo que haya acá.
PERSONAJES = ["kira"]

# ------------------- Puertos / serial -------------------
SERIAL_BAUD = 115200

# ------------------- RAG de memoria (local-first) -------------------
# El índice derivado vive fuera del formato fuente JSONL. FastEmbed descarga
# y cachea el modelo local sólo cuando está habilitado y disponible.
RAG_ENABLED = os.getenv("KIRA_RAG_ENABLED", "1").strip().lower() not in {"0", "false", "no", "off"}
RAG_PROVIDER = os.getenv("KIRA_RAG_PROVIDER", "fastembed")
RAG_MODEL = os.getenv("KIRA_RAG_MODEL", "intfloat/multilingual-e5-small")
RAG_MODEL_FILE = os.getenv("KIRA_RAG_MODEL_FILE", "onnx/model_O4.onnx")
RAG_CACHE_DIR = os.getenv(
    "KIRA_RAG_CACHE_DIR",
    os.path.join(os.path.expanduser("~"), ".cache", "kira", "fastembed"),
)
RAG_ALLOW_DOWNLOAD = os.getenv("KIRA_RAG_ALLOW_DOWNLOAD", "1").strip().lower() not in {
    "0", "false", "no", "off",
}
RAG_BATCH_SIZE = int(os.getenv("KIRA_RAG_BATCH_SIZE", "16"))
RAG_THREADS = int(os.getenv("KIRA_RAG_THREADS", "2"))
RAG_RRF_K = int(os.getenv("KIRA_RAG_RRF_K", "60"))
# E5-small is trained for relative ranking and its useful scores commonly sit
# around 0.7-1.0 (see the model card); 0.84 was too strict for Kira's short
# Spanish memories. Keep this configurable while allowing semantic neighbors.
RAG_MIN_SIMILARITY = float(os.getenv("KIRA_RAG_MIN_SIMILARITY", "0.80"))
# Sin una coincidencia FTS que corrobore, el vectorial exige mayor precisión.
RAG_VECTOR_ONLY_MIN_SIMILARITY = float(
    os.getenv("KIRA_RAG_VECTOR_ONLY_MIN_SIMILARITY", "0.84")
)
RAG_SEMANTIC_DEDUP = os.getenv("KIRA_RAG_SEMANTIC_DEDUP", "1").strip().lower() not in {
    "0", "false", "no", "off",
}
RAG_DEDUP_THRESHOLD = float(os.getenv("KIRA_RAG_DEDUP_THRESHOLD", "0.97"))

# ------------------- VAD Silero (opcional/legacy) -------------------
# La escucha del micro:bit es manual: A envía y B cancela. Estas constantes
# quedan disponibles para herramientas futuras, pero api_escuchar no las usa.
VAD_MODELO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "models", "silero_vad.onnx")
VAD_PROB_VOZ = 0.5          # prob >= esto => frame con voz (inicio)
VAD_PROB_SILENCIO = 0.35    # prob <= esto => silencio real (histeresis)
VAD_MIN_VOZ_MS = 250        # habla sostenida minima para confirmar el turno
VAD_SILENCIO_FIN_MS = 800   # silencio neuronal sostenido => FIN del turno
VAD_SIN_VOZ_MAX_MS = 8000   # nadie hablo nunca => cortar (server-side)
VAD_ESCUCHA_MAX_MS = 25000  # tope total de la escucha (server-side)

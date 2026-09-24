"""Runner de la suite determinista de Kira.

No hace llamadas a proveedores, no abre serial y no usa memoria real salvo que
un fixture temporal sea indispensable. Los smoke tests live están separados en
``run_live_tests.py`` y requieren ``--yes`` explícito.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent

DETERMINISTIC = (
    ("test_app_contract.py", ()),
    ("test_services.py", ()),
    ("test_rag.py", ()),
    ("test_identity.py", ()),
    ("test_memory_policy.py", ()),
    ("test_hardware.py", ()),
    ("test_relay.py", ()),
    ("test_respaldo_ia.py", ()),
    ("test_herramientas.py", ()),
    ("test_stream_deterministic.py", ()),
    ("test_vision_deterministic.py", ()),
    ("test_memoria.py", ("--solo-unitarios",)),
    ("test_reflexion.py", ("--solo-unitarios",)),
    ("test_diario.py", ("--solo-unitarios",)),
)


def main() -> int:
    env = os.environ.copy()
    env["KIRA_SIN_SERIAL"] = "1"
    env["KIRA_RAG_PROVIDER"] = "none"
    env["KIRA_RAG_ALLOW_DOWNLOAD"] = "0"
    env["KIRA_AUTH_REQUIRED"] = "0"
    failures: list[str] = []
    for script, args in DETERMINISTIC:
        command = [sys.executable, script, *args]
        print(f"\n===== {script} {' '.join(args)} =====", flush=True)
        completed = subprocess.run(command, cwd=ROOT, env=env, check=False)
        if completed.returncode:
            failures.append(script)
    print("\n===== RESUMEN DETERMINISTA =====")
    if failures:
        print("FALLARON:", ", ".join(failures))
        return 1
    print(f"OK: {len(DETERMINISTIC)} suites deterministas")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

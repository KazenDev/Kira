"""Smoke tests live de Kira, separados de la suite determinista.

Estos tests pueden llamar a proveedores, consumir saldo y tocar fixtures de
memoria. No se ejecutan automáticamente. Para correrlos:

    .venv/bin/python run_live_tests.py --yes
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
LIVE = (
    "test_hogar.py",
    "test_stream.py",
    "test_vision.py",
    "test_memoria.py",
    "test_recuerdos.py",
    "test_reflexion.py",
    "test_diario.py",
)


def main() -> int:
    if "--yes" not in sys.argv:
        print("Smoke tests live omitidos. Usa --yes si aceptas llamadas externas y fixtures.")
        print("Scripts:", ", ".join(LIVE))
        return 0
    env = os.environ.copy()
    env["KIRA_SIN_SERIAL"] = "1"
    failures: list[str] = []
    for script in LIVE:
        print(f"\n===== LIVE {script} =====", flush=True)
        if subprocess.run([sys.executable, script], cwd=ROOT, env=env, check=False).returncode:
            failures.append(script)
    if failures:
        print("\nLive tests fallaron:", ", ".join(failures))
        return 1
    print("\nLIVE OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

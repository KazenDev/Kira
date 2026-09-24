@echo off
REM ============================================================================
REM  KIRA & KIRO - Launcher para WINDOWS (con diagnostico)
REM  Doble clic y listo. Si algo falta, te dice EXACTAMENTE que instalar.
REM ============================================================================
chcp 65001 >nul
title KIRA & KIRO - Cerebro
cd /d "%~dp0"

echo.
echo  ============================================================
echo    KIRA & KIRO - El Cerebro (Windows)
echo  ============================================================
echo.

REM --- 1) Buscar Python (py launcher o python) ---
set "PY="
where py >nul 2>nul && set "PY=py"
if not defined PY (
    where python >nul 2>nul && set "PY=python"
)
if not defined PY (
    echo  [ERROR] No se encontro Python.
    echo.
    echo  PASOS PARA ARREGLARLO:
    echo   1. Anda a https://www.python.org/downloads/
    echo   2. Baja el instalador y ejecutalo
    echo   3. IMPORTANTE: marca la casilla "Add Python to PATH"
    echo   4. Cierra esta ventana y volve a hacer doble clic en KIRA.bat
    echo.
    pause
    exit /b 1
)

REM --- 2) Version de Python (necesitamos 3.9+) ---
echo  [1/4] Python: %PY%
%PY% --version
%PY% -c "import sys; sys.exit(0 if sys.version_info >= (3,9) else 1)" >nul 2>nul
if errorlevel 1 (
    echo  [ERROR] Python es muy viejo. Necesitas Python 3.9 o superior.
    echo  Bajalo de https://www.python.org/downloads/ y reinstala.
    echo.
    pause
    exit /b 1
)

REM --- 3) Dependencias ---
echo  [2/4] Verificando dependencias...
%PY% -c "import fastapi, uvicorn, httpx, serial" >nul 2>nul
if errorlevel 1 (
    echo        Instalando dependencias (primera vez, tarda un poco)...
    %PY% -m pip install -r "Backend\requirements.txt" --quiet
    if errorlevel 1 (
        echo  [ERROR] Fallo al instalar dependencias.
        echo  Posibles causas:
        echo   - No hay internet
        echo   - pip no esta: instala Python de nuevo marcando "Add to PATH"
        echo  Probaste a mano con:
        echo     %PY% -m pip install -r "Backend\requirements.txt"
        echo.
        pause
        exit /b 1
    )
) else (
    echo        Ya estan instaladas.
)

REM --- 4) Arrancar (el server abre el navegador solo) ---
echo  [3/4] Levantando el Cerebro...
echo        (deja esta ventana abierta: Kira y Kiro viven aca adentro)
echo  [4/4] Abriendo http://127.0.0.1:8000 en tu navegador...
echo.
%PY% "Backend\kira_server.py"
echo.
echo  El servidor se cerro. Chao.
pause

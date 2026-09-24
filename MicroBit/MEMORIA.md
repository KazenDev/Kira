# MEMORIA.md — Mapa de RAM/Flash del micro:bit v2 (auditoría 04-sep-2026)

> La verdad oficial de la memoria, medida con símbolos del ELF (no con el
> reporte del build, que miente — ver abajo). Re-auditable en 2 comandos.

## 🧠 El mapa REAL (nRF52833: 128 KB RAM / 512 KB flash)

| Zona | Tamaño | Qué es |
|---|---|---|
| 🔵 SoftDevice (BLE) | 8.2 KB | Stack Bluetooth de Nordic (RAM 0x20000000 → 0x20002040, reservada desde el boot) |
| 🎨 Firmware estático | **~10.9 KB** | TODAS las variables globales (data+bss, de 0x20002040 a `__bss_end__`=0x20004B14) |
| 💪 Heap libre | **~107 KB** | Memoria dinámica (ManagedStrings, fibras, buffers BLE en runtime) — apenas se usa |
| 🪜 Stack | 2 KB | Reservado arriba (`ASSERT` del linker: heap no invade stack) |

**Uso estático real: ~9%.** El firmware anda Holgado, no apretado.

## 🚨 EL MITO DEL 98.33% (lección de auditoría)

El reporte del build imprime `RAM: 120768 B / 122816 B (98.33%)` y ASUSTA.
Es un cuento: el linker declara todo el espacio restante como sección
`.heap (NOLOAD)` y el reporte cuenta esa RESERVA como uso. O sea: cuenta la
memoria DISPONIBLE como gastada. El 98% era la fiesta, no el apocalipsis.

(Secuela histórica: por ese número se sospechó que el sonido del saludo BLE
mataba la conexión por OOM — falso, el asesino real eran los UUIDs de
escritura/notificación invertidos en el puente web. El sonido era inocente.)

## 🔬 Los comedores de los 10.9 KB estáticos

```
4,976 B  uBit           ← el objeto-dios de CODAL: display, botones, serial,
                           audio, acelerómetro, mic, radio... todo vive ahí
  512 B  im / re / win  ← FFT del micrófono (reales + imaginarios + ventana)
~1.5 KB  m_* (Nordic)   ← estado del SoftDevice (observers, conexiones, colas)
 resto   buffers serial, réplica LED, tablas de animación, metrónomo...
```

**Detalle elegante:** las 8 caritas, loadings y todas las animaciones viven
en FLASH (const/tablas en 512KB, ~31% usado), NO en RAM. Decisión de
arquitectura que resultó ser la correcta.

## 🐍 vs MicroPython (dato pa' el jurado)

| | MicroPython | Este firmware (CODAL C++) |
|---|---|---|
| Modelo | Intérprete en RAM + heap de objetos + GC | Código nativo compilado |
| RAM al arrancar | ~90-100 KB solo por existir | ~11 KB con todo el show |
| Animaciones | En RAM como objetos | En FLASH |
| Libre para el programa | poquito (experiencia propia xd) | ~107 KB |

## 🔁 Cómo re-auditar (si algo raro pasa)

```bash
# fin del bss (uso estático) + tope de stack:
nm build/MICROBIT | grep -E "__bss_end__|__StackTop"

# top comedores de RAM ordenados:
nm --size-sort -t d build/MICROBIT | grep -E " [bB] " | tail -15

# el layout lo define: libraries/codal-microbit-v2/ld/nrf52833-softdevice.ld
# (NOINIT en 0x20002030, RAM app desde 0x20002040, heap hasta stack-0x800)
```

## 📌 Regla pa' el futuro

Si el build dice "RAM 9x%": NO entrar en pánico. Mirar `__bss_end__`.
Si algún día el estático de verdad se pasa de ~120KB, el linker truena con
"heap region overflowed into stack" — ESE sí es el aviso real.

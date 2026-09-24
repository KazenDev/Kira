# 🎨 Rediseño de la Interfaz — Kira

> **Estilo:** *Dark warm minimal / token-based* — base gris-casi-negra cálida, texto blanco-suave, **un solo acento terracota desaturado** (`#d97757`), jerarquía por tono de superficie (no gradientes ni sombras). Formato de conversación tipo Claude/GPT: columna centrada (720px), asistente **sin burbuja**, usuario en píldora.

**Reglas duras:** CERO degradados, CERO `background-clip:text`, nada de `#000`/`#fff` puros, un solo acento, elevación por tono de superficie. Mantener la estructura two-pane existente; solo cambia el sistema visual.

---

## 🧱 Design tokens (la base de todo)

```css
:root {
  /* Superficies: gris cálido, NUNCA #000 puro; elevación = más claro */
  --bg:        #191817;   /* fondo del chat */
  --elev-1:    #151413;   /* sidebar */
  --elev-2:    #211f1d;   /* composer, hover, burbuja usuario */
  --elev-3:    #2b2825;   /* activo/chips, disabled */
  --border:    rgba(255,255,255,.08);

  /* Texto: NUNCA #fff puro */
  --text-1: #e8e6e3;      /* principal */
  --text-2: #b1ada1;      /* secundario (Cloudy de Claude) */
  --text-3: #8a867c;      /* timestamps, placeholders */

  /* UN acento, desaturado y aclarado para oscuro */
  --accent:      #d96f55;             /* terracota de Kira */
  --accent-hover:#e58268;
  --accent-soft: rgba(217,111,85,.14); /* fondos de foco/activo */
  --ok:  #4fb286;                      /* verde desaturado p/ micro:bit */

  --r-sm:10px; --r-md:14px; --r-lg:20px; --r-pill:999px;
  --font:"Inter",system-ui,-apple-system,"Segoe UI",sans-serif;
  --t-fast:160ms; --ease:cubic-bezier(.2,0,.4,1);
}
```

Tipografía: **system-ui** (sin Inter instalado, es lo que permite el spec), escala 12/14/16; texto del asistente `16px/1.7`; UI `13–14px`. Jerarquía por color (`--text-2/3`), no negritas everywhere.

---

## 📋 Fases de implementación

### ✅ Fase 0 — Tokens y reset (estilos.css)
- Reescribir `estilos.css` completo: tokens en `:root`, reset, scrollbar finita (8px, thumb `#3a3733`), `:focus-visible` con outline 2px `--accent`, `prefers-reduced-motion` que apaga todo.
- Eliminar el tema claro WhatsApp (fondo beige con garabatos, gradientes, violeta).
- **Estado:** ✅ hecho

### ✅ Fase 1 — Burbuja → formato "turn" (Claude/GPT)
- `Burbuja.tsx`: asistente **sin burbuja** — avatar 30px a la izquierda + texto a ancho completo (720px, `16px/1.7`). Usuario en **píldora** `--elev-2` radio 20px alineada a la derecha.
- Timestamps **fuera** de las burbujas, 11px `--text-3`.
- Fuentes de la web: card con borde `--border`, radio `--r-md`, numeradas.
- **Acciones al hover** sobre el mensaje del asistente: `copy`/`check` (copiar texto).
- **Estado:** ✅ hecho

### ✅ Fase 2 — Header del chat
- `ChatHeader.tsx`: alto 56px, fondo `--bg`, borde inferior `--border` (no tarjeta flotante).
- Avatar 32px plano (sin degradado), nombre 15px semibold.
- Estado del micro:bit como **pill mínima** con punto verde `--ok` que **pulsa** (2s).
- El panel LED "En Vivo" deja de flotar: botón con icono `cpu` que abre un **popover** con la matriz del micro:bit.
- **Estado:** ✅ hecho

### ✅ Fase 3 — Composer (barra de entrada)
- `App.tsx`: píldora radio 24–28px, fondo `--elev-2`, `:focus-within` → borde `--accent` + anillo `--accent-soft`.
- Botón de envío: **círculo `--accent` con icono `arrow-up`**. Disabled = `--elev-3` + `--text-3`.
- Mic como icon-button fantasma (`mic`). (No se agregó `plus` para adjuntar: sería un botón muerto sin funcionalidad real.)
- **Estado:** ✅ hecho

### ✅ Fase 4 — Sidebar
- `Sidebar.tsx`: fondo `--elev-1` + `border-right --border`. Logo **plano** (sin gradiente, sin `background-clip:text`).
- Buscador "fantasma" (`--elev-2`, icono `search`), chips ghost con borde `--border` (activo = `--accent-soft` + texto `--accent`).
- Ítems de conversación: radio 12px, hover `--elev-2`, activo `--accent-soft` + barrita 2px `--accent`. Preview a una línea `--text-3`.
- Footer: **un solo** indicador micro:bit (punto `--ok` + texto 12px), sync como icon-button `refresh-cw` que gira mientras sincroniza.
- **Estado:** ✅ hecho

### ✅ Fase 5 — Bienvenida, Settings y detalles
- Hero: logo plano, título sin degradado, subtítulo `--text-2`, tarjetas en dark (radio `--r-lg`, hover elevación).
- Settings modal adaptado a tokens dark (ya usa clases propias → solo CSS).
- Iconos: set único Lucide — `plus`, `arrow-up`, `mic`, `settings`, `refresh-cw`, `search`, `panel-left`, `cpu`, `copy`, `check`. 20px UI / 16px chicos, stroke 1.75–2, `currentColor` con `--text-2`.
- **Estado:** ✅ hecho

### ✅ Fase 6 — Micro-interacciones y pulido
- Typing indicator: 3 puntos + *"Kiro está escribiendo…"* (el detalle más "humano").
- Mensajes entran con fade + `translateY(4px)` (160ms), stagger sutil.
- Punto de estado del micro:bit con pulso suave (2s).
- **Estado:** ✅ hecho

### ✅ Fase 7 — Compilar y verificar
- `tsc && vite build` (script `build`) → ✅ compila (2073 módulos, CSS 24.6 kB).
- Server sirviendo el build nuevo (CSS con los tokens `--elev-*` ✅).
- Verificación en navegador: **pendiente de revisión visual del usuario** (recargar con Ctrl+Shift+R en http://127.0.0.1:8000).
- **Estado:** ✅ hecho

---

## 🔍 Notas de la investigación (por qué este diseño)

- **Claude:** paleta cálida terracota `#C15F3C` / crema `#F4F3EE`; dark mode = "conversación de noche, no terminal fría".
- **Dark mode real:** nada de blanco puro (usar `#E8E8E8`), desaturar acentos, elevación por superficies más claras (Material 3), rangos de gris estrechos (ChatGPT `#141414 → #3C3C50`).
- **Lucide:** set único recomendado para UIs minimalistas tipo shadcn/IA. Nunca mezclar sets.
- **Lo "humano" viene de restar:** superficies planas, un solo acento, columna centrada, tipografía tranquila — no de decorar más.

---

## Fase 8 — Kira como producto real ✅ (23-sep-2026)

La capa V5 ya tenía un chat maduro; esta fase quita el último AI slop del
onboarding y de la navegación sin romper SSE, TTS, BLE ni persistencia.

- **Welcome single-character:** desaparecieron selector, tarjetas, estadísticas
  y copy técnico. Ahora hay marca, una frase concreta, continuar/empezar y tres
  sugerencias accionables.
- **Mobile-first:** el drawer se abre desde la bienvenida, tiene backdrop,
  Escape, botón Inicio y ocupa todo el ancho; nunca queda icon-only.
- **Header:** título real de conversación, estado `Kira · micro:bit`, un acceso
  al LED y un menú agrupado para BLE, recuerdos y actualización. El controlador
  BLE permanece montado en `App` aunque se vuelva al inicio.
- **Concurrencia:** el SSE sigue bloqueando un segundo envío aunque empiece el
  TTS; el replay reutiliza la cola completa y se habilita sólo en reposo.
- **Composer:** superficie rectangular de 16 px, borde neutro en reposo y una
  sola fila incluso en 390 px; el contador sólo aparece cerca del límite.
- **Mensajes:** timestamp fuera de la vista y copiar/regenerar/escuchar dentro
  de un único menú de acciones.
- **Settings/Memoria:** copy maduro, `aria-modal`, foco atrapado, Escape y
  bottom sheet móvil.
- **Marca:** el paquete y las etiquetas accesibles usan sólo Kira; DeepSeek
  queda como proveedor técnico.
- **Estado:** build TypeScript/Vite limpio, 76/76 tests y revisión visual en
  1440×900 + 390×844.

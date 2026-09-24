/**
 * ui-utils.ts — lógica PURA de la interfaz (sin React, sin fetch).
 * Todo testeable con vitest: `npm run test`.
 */

/** Info mínima del micro:bit que necesita el indicador. */
export type InfoMicro = {
  respondiendo?: boolean;
  conectado?: boolean;
  ble_relay?: boolean;
};

/** Estado del indicador del micro:bit (sidebar y encabezado del chat).
 *
 * OJO con `relay`: "placa por BLE" (modo feria: el enlace lo lleva el CELULAR)
 * es un estado BUENO — el enlace vive y la placa contesta por el celular. Antes
 * caía en la rama de error, así que la app mostraba "placa por BLE" con el punto
 * ROJO y sin latido: parecía una falla justo cuando estaba funcionando. */
export function estadoMicrobit(mb?: InfoMicro | null): 'ok' | 'con' | 'relay' | 'mal' {
  if (mb?.respondiendo) return 'ok';
  if (mb?.conectado) return 'con';
  if (mb?.ble_relay) return 'relay';
  return 'mal';
}

/** Texto del indicador del micro:bit. */
export function textoMicrobit(mb?: InfoMicro | null): string {
  if (mb?.respondiendo) return 'micro:bit en línea';
  if (mb?.conectado) return 'conectado...';
  if (mb?.ble_relay) return 'placa por BLE';
  return 'sin micro:bit';
}

/** ¿El scroll está "al fondo"? (umbral 90px: patrón estándar de chats).
 * Extraída de revisarScroll para poder testearla. */
export function estaAlFondo(
  scrollHeight: number,
  scrollTop: number,
  clientHeight: number,
  umbral = 90
): boolean {
  return scrollHeight - scrollTop - clientHeight < umbral;
}

/** Sugerencias del chat vacío (romper el hielo, patrón de activación).
 * Tocan las 3 superpotencias: charla, micro:bit físico y sensores. */
export const SUGERENCIAS_INICIO: string[] = [
  'Hola, ¿quién sos?',
  'Poné el metrónomo a 90',
  '¿Qué temperatura hace allá?',
];

/** ¿Cuál de los dos indicadores de "escribiendo" va AHORA?
 *
 * El mensaje parcial nace VACÍO apenas mandás el mensaje (y también al
 * regenerar). Mientras esté vacío, el indicador correcto son los PUNTITOS;
 * apenas llega la primera palabra, la burbuja con el cursor parpadeante.
 *
 * El bug (captura del 17-sep): se dibujaban los dos A LA VEZ — la burbuja
 * vacía con su cursor "|" y el pie "escribiendo… HH:MM", y más abajo los
 * puntitos de "X está escribiendo…". La app entera parecía duplicada.
 *
 *   'puntos'  -> todavía no hay ni una palabra: la burbuja vacía se OCULTA
 *   'burbuja' -> ya hay texto: la burbuja se dibuja con el cursor
 *   null      -> no hay respuesta en curso
 */
export function indicadorEscribiendo(
  enCurso: boolean,
  idParcial: string | null,
  mensajes: { id: string; rol: string; contenido: string }[],
  rolPersonaje?: string
): 'puntos' | 'burbuja' | null {
  if (!enCurso || !idParcial) return null;
  const parcial = mensajes.find((m) => m.id === idParcial);
  if (!parcial) return null;
  if (rolPersonaje && parcial.rol !== rolPersonaje) return null;
  return parcial.contenido.trim() ? 'burbuja' : 'puntos';
}

/** Estado NOMBRADO de tool (patrón "named tool states"): mientras la IA
 * usa una herramienta (sensor, web, cálculo...), el slot de los puntitos
 * muestra QUÉ está haciendo en vez de un "escribiendo…" genérico — el hueco
 * puede durar hasta ~4s (sensor sin placa) y un indicador opaco parece que
 * se colgó. Nombre de tool -> frase en español, con Kira como sujeto afuera.
 */
const LABELS_HERRAMIENTA: Record<string, string> = {
  leer_temperatura: 'leyendo el termómetro…',
  leer_luz: 'midiendo la luz…',
  leer_botones: 'mirando los botones…',
  leer_movimiento: 'sintiendo el movimiento…',
  leer_sonido: 'escuchando el sonido…',
  leer_bateria: 'midiendo la batería…',
  reloj: 'mirando el reloj…',
  buscar_en_web: 'buscando en la web…',
  calcular: 'haciendo la cuenta…',
  controlar_metronomo: 'ajustando el metrónomo…',
  leer_url: 'leyendo esa página…',
  guardar_recuerdo: 'anotándolo en su memoria…',
  azar: 'tirando los dados…',
  leer_estado: 'revisando cómo está…',
};

export function labelHerramienta(nombre: string | null | undefined): string | null {
  if (!nombre) return null;
  return LABELS_HERRAMIENTA[nombre] ?? 'usando una herramienta…';
}

/**
 * UUID v4 con FALLBACK para contextos no seguros.
 *
 * `crypto.randomUUID()` SOLO existe en contextos seguros (https o localhost).
 * Sirviendo la web por http:// en una IP (el modo feria del VPS) el navegador
 * no la expone y la app CRASHEABA al montar (el token del puente BLE y los ids
 * de chat salen de aca).
 *
 * Orden de preferencia: randomUUID -> getRandomValues (que SI esta en http
 * plano) -> Math.random (ultimo recurso, navegadores prehistoricos). Los ids
 * son locales (claves de React/localStorage), no de seguridad, asi que el
 * fallback es correcto y suficiente.
 */
export function uuid(): string {
  const c = globalThis.crypto as Crypto | undefined;
  if (typeof c?.randomUUID === 'function') return c.randomUUID();

  if (typeof c?.getRandomValues === 'function') {
    const b = c.getRandomValues(new Uint8Array(16));
    b[6] = (b[6] & 0x0f) | 0x40; // version 4
    b[8] = (b[8] & 0x3f) | 0x80; // variante RFC 4122
    const hex = Array.from(b, (n) => n.toString(16).padStart(2, '0')).join('');
    return `${hex.slice(0, 8)}-${hex.slice(8, 12)}-${hex.slice(12, 16)}-${hex.slice(16, 20)}-${hex.slice(20)}`;
  }

  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (ch) => {
    const r = (Math.random() * 16) | 0;
    const v = ch === 'x' ? r : (r & 0x3) | 0x8;
    return v.toString(16);
  });
}

/**
 * Tamaño al que se REDUCE una foto de la cámara antes de mandarla.
 *
 * Una foto de un celular moderno (12 MP) pesa 3-5 MB: subirla entera por datos
 * móviles tarda, el data URL de la IA crece al 133% (base64) y, si se guarda en
 * el chat del navegador, llena el localStorage (5 MB) en dos fotos. A ~1024 px
 * de lado máximo queda en ~200 KB y la IA la ve igual de bien (con más de 1024
 * px no gana nada: el propio modelo reescala la imagen a ~1300x1300).
 *
 * Devuelve el par ya redondeado; nunca agranda una foto chica.
 */
export function dimensionesEscaladas(
  ancho: number,
  alto: number,
  maxLado: number
): { ancho: number; alto: number } {
  const w = Math.max(1, Math.round(Number(ancho) || 0));
  const h = Math.max(1, Math.round(Number(alto) || 0));
  const tope = Math.max(1, Math.round(Number(maxLado) || 0));
  const mayor = Math.max(w, h);
  if (mayor <= tope) return { ancho: w, alto: h };   // ya es chica: no se agranda
  const factor = tope / mayor;
  return {
    ancho: Math.max(1, Math.round(w * factor)),
    alto: Math.max(1, Math.round(h * factor)),
  };
}

/** Un fragmento del texto del sidebar y si coincide con la búsqueda.
 *
 * `resaltarPartes` devuelve pedazos que, concatenados, reconstruyen el
 * texto ORIGINAL: el resaltado es solo presentación, nunca pierde letras.
 * Case-insensitive (el filtro del sidebar hace lo mismo con toLowerCase).
 * La búsqueda se escapa antes de armar el RegExp: es texto LITERAL del
 * usuario — "(2+2)" o "a.b" tienen que matchear tal cual, no como regex. */
export type ParteResaltada = { t: string; coincide: boolean };

export function resaltarPartes(texto: string, busqueda: string): ParteResaltada[] {
  const q = busqueda.trim();
  if (!q || !texto) return [{ t: texto, coincide: false }];

  const escapado = q.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const re = new RegExp(escapado, 'gi');
  const partes: ParteResaltada[] = [];
  let ultimo = 0;
  let m: RegExpExecArray | null;
  while ((m = re.exec(texto)) !== null) {
    if (m.index > ultimo) partes.push({ t: texto.slice(ultimo, m.index), coincide: false });
    partes.push({ t: m[0], coincide: true });
    ultimo = m.index + m[0].length;
    if (m[0].length === 0) re.lastIndex++; // guard: coincidencia vacía infinita
  }
  if (ultimo < texto.length) partes.push({ t: texto.slice(ultimo), coincide: false });
  return partes.length ? partes : [{ t: texto, coincide: false }];
}

/** Fecha corta es-CO para recuerdos ("7 sep, 13:34"). */
export function formatearFechaCorta(iso: string): string {
  try {
    const d = new Date(iso);
    if (Number.isNaN(d.getTime())) return iso;
    const meses = [
      'ene', 'feb', 'mar', 'abr', 'may', 'jun',
      'jul', 'ago', 'sep', 'oct', 'nov', 'dic',
    ];
    const hh = String(d.getHours()).padStart(2, '0');
    const mm = String(d.getMinutes()).padStart(2, '0');
    return `${d.getDate()} ${meses[d.getMonth()]}, ${hh}:${mm}`;
  } catch {
    return iso;
  }
}

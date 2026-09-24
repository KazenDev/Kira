/**
 * foto.ts — LA CÁMARA DEL CELULAR.
 *
 * El botón 📷 del input abre la cámara nativa (`<input type="file"
 * accept="image/*" capture="environment">`: en el celular saca la foto ahí
 * mismo, en la compu abre el selector de archivos). Acá vive el procesamiento:
 * leer el archivo, REDUCIRLO y devolverlo como data URL lista para la IA.
 *
 * Por qué se reduce en el navegador y no en el servidor:
 *   - el celular del stand sube por datos móviles: 200 KB sube al toque, 4 MB no
 *   - el data URL viaja al backend y de ahí a DeepSeek (base64 = +33%)
 *   - la burbuja del chat se guarda en localStorage (5 MB): con fotos de 4 MB
 *     el chat dejaría de guardarse para siempre
 *
 * MEMORIA — ESTO ES LO QUE EVITA QUE EL CELULAR SE MUERA:
 *   una foto de 12 MP decodificada ocupa ~48 MB de RAM. Decodificarla ENTERA
 *   solo para reducirla es justo lo que congela (y a veces mata) el navegador de
 *   un celular de gama media. Por eso:
 *     1. `createImageBitmap(archivo, { resizeWidth })` decodifica UNA sola vez y
 *        ya al tamaño final (el navegador escala dentro del decodificador).
 *     2. El bitmap se CIERRA apenas se dibuja (`close()`) y el canvas queda del
 *        tamaño final (≤1024 px ≈ 5 MB, no 48).
 *     3. La imagen original NUNCA se guarda en el estado de React: lo que queda
 *        es el JPEG reducido (~200 KB).
 *     4. Un archivo absurdo (>25 MB) se rechaza con aviso, sin decodificarlo.
 *     5. Si algo falla, se libera TODO (bitmap/object URL/canvas) igual: la app
 *        sigue viva y no queda memoria colgada.
 */
import { dimensionesEscaladas } from './ui-utils';

/** Lado máximo de la foto que se manda (px). */
export const FOTO_LADO_MAX = 1024;
/** Calidad JPEG inicial (baja más si el resultado es muy pesado). */
export const FOTO_CALIDAD = 0.72;
/** Más pesado que esto ni se intenta procesar (protege la RAM del celular). */
export const FOTO_PESO_MAX = 25 * 1024 * 1024;
/** Calidades probadas de mayor a menor hasta entrar en el tope. */
const CALIDADES = [FOTO_CALIDAD, 0.55, 0.4];
/** Tope del data URL: ~900 KB de foto real (entra cómodo en localStorage y en la API). */
const LIMITE_DATA_URL = 1_200_000;

type Fuente = {
  imagen: ImageBitmap | HTMLImageElement;
  ancho: number;
  alto: number;
  /** Suelta la memoria de la imagen original (obligatorio llamarlo). */
  liberar: () => void;
};

function cargarImagenElemento(url: string): Promise<HTMLImageElement> {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.decoding = 'async';
    img.onload = () => resolve(img);
    img.onerror = () => reject(new Error('No se pudo leer la foto'));
    img.src = url;
  });
}

/**
 * Abre la foto de la forma más liviana posible. Primero por el camino rápido
 * (createImageBitmap escalando), y si el navegador no puede, por el clásico
 * `<img>` + object URL.
 */
async function abrirFuente(archivo: File): Promise<Fuente> {
  if (typeof createImageBitmap === 'function') {
    try {
      // `as unknown as` porque `imageOrientation` no está en todos los libs de TS
      const opciones = {
        resizeWidth: FOTO_LADO_MAX,
        resizeQuality: 'high',
        imageOrientation: 'from-image', // respeta el giro EXIF de la cámara
      } as unknown as ImageBitmapOptions;
      const bitmap = await createImageBitmap(archivo, opciones);
      return {
        imagen: bitmap,
        ancho: bitmap.width,
        alto: bitmap.height,
        liberar: () => bitmap.close(),
      };
    } catch {
      /* navegador viejo o formato raro: se intenta el camino clásico */
    }
  }

  const url = URL.createObjectURL(archivo);
  try {
    const img = await cargarImagenElemento(url);
    const ancho = img.naturalWidth || img.width;
    const alto = img.naturalHeight || img.height;
    if (!ancho || !alto) throw new Error('La foto vino vacía');
    return {
      imagen: img,
      ancho,
      alto,
      liberar: () => {
        img.src = 'data:,'; // suelta el bitmap decodificado del navegador
        URL.revokeObjectURL(url);
      },
    };
  } catch (e) {
    URL.revokeObjectURL(url);
    throw e;
  }
}

/**
 * Convierte el archivo que devolvió la cámara en un data URL JPEG reducido.
 * Lanza si el archivo no es una imagen usable (el que llama muestra el aviso).
 */
export async function fotoDesdeArchivo(archivo: File): Promise<string> {
  if (!archivo.type.startsWith('image/')) {
    throw new Error('Eso no es una imagen');
  }
  if (archivo.size > FOTO_PESO_MAX) {
    throw new Error('La foto es demasiado pesada');
  }

  const fuente = await abrirFuente(archivo);
  let canvas: HTMLCanvasElement | null = null;
  try {
    const { ancho, alto } = dimensionesEscaladas(fuente.ancho, fuente.alto, FOTO_LADO_MAX);
    canvas = document.createElement('canvas');
    canvas.width = ancho;
    canvas.height = alto;
    const ctx = canvas.getContext('2d');
    if (!ctx) throw new Error('Este navegador no puede procesar la foto');
    // fondo blanco: si la foto tiene transparencia (PNG), el JPEG no la soporta
    ctx.fillStyle = '#fff';
    ctx.fillRect(0, 0, ancho, alto);
    ctx.drawImage(fuente.imagen, 0, 0, ancho, alto);

    let salida = canvas.toDataURL('image/jpeg', CALIDADES[0]);
    for (const calidad of CALIDADES.slice(1)) {
      if (salida.length <= LIMITE_DATA_URL) break;
      salida = canvas.toDataURL('image/jpeg', calidad);
    }
    if (!salida.startsWith('data:image/jpeg') || salida.length < 1000) {
      throw new Error('La foto salió vacía');
    }
    return salida;
  } finally {
    // liberar SIEMPRE: sin esto, dos o tres fotos seguidas dejan al celular
    // sin memoria (y el navegador recarga la pestaña sola)
    fuente.liberar();
    if (canvas) {
      canvas.width = 1;
      canvas.height = 1;
    }
  }
}

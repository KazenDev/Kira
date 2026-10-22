// Ajustes locales del usuario (voz, autonomía, fondo, memoria).
// Viven en localStorage para que la app recuerde cómo la dejaste.
import {
  PREFERENCIAS_MEMORIA_POR_DEFECTO,
  type PreferenciasMemoria,
} from './api';

export type AjustesLocales = {
  voz: boolean;
  volumenVoz: number;
  memoria: PreferenciasMemoria;
  autonomia: boolean; // Kira habla sola (eventos de la placa, silencios)
  fondoVivo: boolean; // el resplandor WebGL de fondo (apagable en celus viejos)
};

const STORAGE_AJUSTES = 'kira_ajustes_v1';

export const AJUSTES_INICIALES: AjustesLocales = {
  voz: true,
  volumenVoz: 0.8,
  autonomia: true,
  fondoVivo: true,
  memoria: { ...PREFERENCIAS_MEMORIA_POR_DEFECTO },
};

export function cargarAjustes(userId: string): AjustesLocales {
  try {
    const raw = localStorage.getItem(`${STORAGE_AJUSTES}_${userId}`);
    if (!raw) return { ...AJUSTES_INICIALES, memoria: { ...AJUSTES_INICIALES.memoria } };
    const parsed = JSON.parse(raw) as Partial<AjustesLocales>;
    const memoria = parsed.memoria && typeof parsed.memoria === 'object'
      ? parsed.memoria as Partial<PreferenciasMemoria>
      : {};
    const volumen = Number(parsed.volumenVoz);
    return {
      voz: parsed.voz !== false,
      autonomia: parsed.autonomia !== false,
      fondoVivo: parsed.fondoVivo !== false,
      volumenVoz: Number.isFinite(volumen)
        ? Math.max(0, Math.min(1, volumen))
        : AJUSTES_INICIALES.volumenVoz,
      memoria: {
        recordar: memoria.recordar !== false,
        hechos: memoria.hechos !== false,
        experiencias: memoria.experiencias !== false,
        usar: memoria.usar !== false,
      },
    };
  } catch {
    return { ...AJUSTES_INICIALES, memoria: { ...AJUSTES_INICIALES.memoria } };
  }
}

export function guardarAjustes(userId: string, valores: AjustesLocales): void {
  try {
    localStorage.setItem(`${STORAGE_AJUSTES}_${userId}`, JSON.stringify(valores));
  } catch {
    // Ajustes sigue funcionando en memoria si localStorage está bloqueado.
  }
}

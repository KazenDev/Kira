// Volcado diferido: agrupa muchas llamadas en una sola escritura.
// Pensado para localStorage durante el streaming (30 cambios por segundo).

export type VolcadoDiferido<T> = {
  /** Programa una escritura; reinicia el reloj si ya había una pendiente. */
  programar: (valor: T) => void;
  /** Escribe YA si había algo pendiente (al desmontar u ocultar la pestaña). */
  volcar: () => void;
  /** Cancela sin escribir (limpieza). */
  cancelar: () => void;
  /** ¿Hay una escritura esperando? (tests y diagnóstico) */
  pendiente: () => boolean;
};

export function crearVolcadoDiferido<T>(
  escribir: (valor: T) => void,
  ms: number
): VolcadoDiferido<T> {
  let timer: ReturnType<typeof setTimeout> | null = null;
  let ultimo: T;
  let hayValor = false;

  const limpiar = () => {
    if (timer) clearTimeout(timer);
    timer = null;
  };

  return {
    programar(valor) {
      ultimo = valor;
      hayValor = true;
      limpiar();
      timer = setTimeout(() => {
        timer = null;
        if (!hayValor) return;
        hayValor = false;
        escribir(ultimo);
      }, ms);
    },
    volcar() {
      limpiar();
      if (!hayValor) return;
      hayValor = false;
      escribir(ultimo);
    },
    cancelar() {
      limpiar();
      hayValor = false;
    },
    pendiente: () => hayValor,
  };
}

// Tokens de motion compartidos por toda la app: duraciones, easings y
// springs en UN solo lugar. Regla anti-"slop": ninguna animación con valores
// bespoke sueltos (153ms, cubic-bezier inventado, springs repetidos a mano).
// Escalas según research (NN/g, Material, UI Craft): micro <150ms,
// base ~200ms, media ~280ms, grande ~400ms; salidas más rápidas que entradas.

/** Duraciones estándar (segundos, formato de motion/react). */
export const T = {
  micro: 0.14, // feedback inmediato (hover, press, crossfade chico)
  fast: 0.16,  // transiciones chicas: tooltips, labels, toasts
  med: 0.26,   // UI media: entradas de lista, paneles chicos
  slow: 0.38,  // entradas grandes: hero, tarjetas (nunca > 0.4)
} as const;

/** Stagger entre hermanos de una misma lista (research: 30–80ms). */
export const STAGGER = 0.06;

/** Ease-out expo: la curva de ENTRADAS (decelera al llegar). Equivale al
 *  --ease-out del CSS para que las dos capas compartan la misma curva. */
export const EASE_OUT: [number, number, number, number] = [0.16, 1, 0.3, 1];

/** Springs por contexto: uno por tipo de elemento, no uno por componente. */
export const SPRING_MSG = { type: 'spring', stiffness: 520, damping: 32, mass: 0.9 } as const;
export const SPRING_MODAL = { type: 'spring', stiffness: 480, damping: 34 } as const;
export const SPRING_POP = { type: 'spring', stiffness: 430, damping: 32, mass: 0.72 } as const;

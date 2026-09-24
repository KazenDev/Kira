/**
 * enlace-ble.ts — LA GUARDIA DEL ENLACE BLE 🛡️📡
 *
 * El sintoma del 17-sep (tarde), medido en el log del server:
 *
 *     >> comandos que salieron al celu: 28   (CALLA, TALK, HAPPY, SENSOR:TEMP...)
 *     << cosas que volvieron de la placa: 1  (un unico ACK:CALLA)
 *
 * Escribiamos y no volvia NADA: el `SENSOR:TEMP` salia del servidor, llegaba al
 * celu... y el `TEMP:26.5` nunca aparecia. La app no se enteraba: los
 * `writeValue` resolvian sin error (o quedaban colgados para siempre, bug
 * documentado de Web Bluetooth en Android despues de reconectar) y las
 * notificaciones seguian apagadas.
 *
 * Las notificaciones BLE (CCCD) las maneja la APP, no el stack: en Android
 * basta que OTRA pestana haga stopNotifications/startNotifications sobre la
 * misma placa para dejar muda a la que estaba andando. Y Chrome no avisa:
 * `gattserverdisconnected` es lo unico que promete (web-bluetooth#500).
 *
 * Entonces: si mandamos comandos y la placa no contesta en X segundos,
 * re-armamos el enlace (GATT fresco + startNotifications otra vez). Esta
 * funcion decide CUANDO, sin tocar el DOM: asi se puede testear.
 *
 * Correr: npm run test
 */

/** Los tiempos de la guardia (defaults de produccion). */
export const SILENCIO = {
  /** sin NADA de la placa por este rato (mandando comandos) = enlace mudo */
  silencioMs: 12_000,
  /** no re-armamos dos veces mas seguido que esto (evita pelearse solo) */
  rearmeMinMs: 30_000,
} as const;

export interface ConfigGuardia {
  silencioMs: number;
  rearmeMinMs: number;
}

export interface DatosEnlace {
  /** ¿El server nos dio el puesto de puente? (sin esto no somos el cable) */
  esDueno: boolean;
  /** ¿Tenemos caracteristicas GATT vivas para escribir? */
  tieneEnlace: boolean;
  /** Comandos escritos desde la ULTIMA respuesta de la placa */
  comandosSinRespuesta: number;
  /** ms desde la ultima señal (respuesta de la placa o nuestro ultimo
   *  comando). `null` = todavia no paso nada en esta sesion */
  msDeSilencio: number | null;
  /** ms desde el ultimo re-arme por silencio (Infinity = nunca) */
  msDesdeUltimoRearme: number;
}

/**
 * ¿Hay que re-armar el enlace? Solo si se juntan TODAS:
 *  1. somos el puente (si no, otro dispositivo es el cable),
 *  2. tenemos enlace GATT (si no hay enlace, esto no es "un enlace mudo",
 *     es otra cosa: lo maneja la reconexion de siempre),
 *  3. mandamos algo y no volvio nada (si nadie pregunto, el silencio es
 *     perfectamente normal: la placa no habla sola),
 *  4. el silencio ya duro lo suficiente,
 *  5. y no re-armamos hace poquito (cooldown: si el enlace esta roto de
 *     verdad, que no se convierta en una tormenta de re-conexiones).
 */
export function debeRearmarEnlace(d: DatosEnlace, cfg: ConfigGuardia = SILENCIO): boolean {
  if (!d.esDueno) return false;
  if (!d.tieneEnlace) return false;
  if (d.comandosSinRespuesta <= 0) return false;
  if (d.msDeSilencio === null) return false;
  if (d.msDeSilencio < cfg.silencioMs) return false;
  if (d.msDesdeUltimoRearme < cfg.rearmeMinMs) return false;
  return true;
}

/** Cuantos ms hace que la placa (o nosotros) no damos señales. `null` si
 *  todavia no hubo ninguna: se calcula desde el evento mas RECIENTE, asi un
 *  comando recien mandado reinicia el reloj (y no re-armamos por las dudas). */
export function msDeSilencio(
  ahora: number,
  ultimaRespuestaMs: number,
  ultimaEscrituraMs: number
): number | null {
  const ultima = Math.max(ultimaRespuestaMs, ultimaEscrituraMs);
  if (ultima <= 0) return null;
  return Math.max(0, ahora - ultima);
}

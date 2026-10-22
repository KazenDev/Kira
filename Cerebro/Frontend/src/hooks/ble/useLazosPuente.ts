// Lazos de vida del puente que NO dependen del estado de la UI: latido del
// puesto, ping liviano y loop de comandos server -> placa.
import { useEffect, useRef } from 'react';
import type { MutableRefObject } from 'react';
import { bleDevolver, blePing, bleTomarTx } from '../../lib/api';
import {
  LATIDO_MS,
  YO,
  dormir,
  escribirLineas,
  type Caracteristicas,
} from '../../lib/ble-protocolo';

type Opciones = {
  token: string;
  esDueno: MutableRefObject<boolean>;
  chars: MutableRefObject<Caracteristicas | null>;
  dispositivo: MutableRefObject<BluetoothDevice | null>;
  ultimaEscrituraMs: MutableRefObject<number>;
  comandosSinRespuesta: MutableRefObject<number>;
  reportar: (mensaje: string) => void;
  rearmar: (motivo: string) => void;
  registrar: () => Promise<boolean>;
  perderPuesto: (motivo: string) => void;
  programarReclamo: () => void;
};

export function useLazosPuente({
  token,
  esDueno,
  chars,
  dispositivo,
  ultimaEscrituraMs,
  comandosSinRespuesta,
  reportar,
  rearmar,
  registrar,
  perderPuesto,
  programarReclamo,
}: Opciones) {
  const rearmarRef = useRef(rearmar);
  rearmarRef.current = rearmar;

  // ---------- TX: server -> placa (long-poll con token) ----------
  // El latido NO depende del estado de la UI: mientras seamos dueños, este
  // loop late. Antes vivía en un efecto atado a `estado` y cualquier
  // transición momentánea lo mataba en silencio (el server soltaba el puente
  // 60s después y todo quedaba mudo: el bug de "se desconecta a los ~10s").
  useEffect(() => {
    let vivo = true;
    const ciclo = async () => {
      let ultimoLatidoTx = 0;
      let vueltas = 0;
      while (vivo) {
        vueltas++;
        // LATIDO DE TRAZABILIDAD: sin esto, si este bucle deja de pedir
        // comandos (o se atasca en un await), desde el VPS solo se ve "el
        // puente está VIVO" y el SENSOR:TEMP muere en la cola sin una sola
        // pista. Pasó el 17-sep: 0 /api/ble/tx durante minutos y ningún
        // aviso. Cada 15s esta copia cuenta en qué estado está.
        if (Date.now() - ultimoLatidoTx > 15000) {
          ultimoLatidoTx = Date.now();
          reportar(
            `diag: tx vivo [${YO}] dueno=${esDueno.current ? 1 : 0} ` +
              `enlace=${chars.current ? 1 : 0} gatt=${dispositivo.current?.gatt?.connected ? 1 : 0} ` +
              `vueltas=${vueltas}`
          );
        }
        if (!esDueno.current) {
          await dormir(250);
          continue;
        }
        const ch = chars.current;
        const gatt = dispositivo.current?.gatt;
        // ¿EL ENLACE SIGUE VIVO DE VERDAD? Chrome puede NO avisar de la caída
        // (`gattserverdisconnected` no siempre llega: está documentado que
        // falta si la placa se va de rango o se resetea). Si el GATT dice que
        // no estamos conectados, seguir escribiendo sería mandar comandos al
        // VACÍO para siempre: la placa queda sin boca, sin cara y sin ACKs y
        // nadie se entera (bug del 17-sep). Acá se re-arma al toque.
        if (!ch || !gatt || !gatt.connected) {
          if (esDueno.current && dispositivo.current) {
            chars.current = null;
            rearmarRef.current('el GATT se cayó sin avisar');
          }
          await dormir(1000);
          continue;
        }
        try {
          const lineas = await bleTomarTx(token, 5);
          if (!vivo) break;
          if (lineas.length && chars.current) {
            try {
              const bytes = new TextEncoder().encode(lineas.join(''));
              await escribirLineas(chars.current.escribir, bytes);
              // Ya hay comandos "esperando respuesta": eso es lo que mide la
              // guardia del enlace para decidir si el camino de vuelta murió.
              ultimaEscrituraMs.current = Date.now();
              comandosSinRespuesta.current += lineas.length;
              // Trazabilidad: sin esto no se puede distinguir "la app NO
              // escribió" de "escribió y la placa no contesta".
              reportar(`diag: escribi ${bytes.length}B '${lineas.join('').trim()}'`);
            } catch (e) {
              // SI LA ESCRITURA FALLA, EL COMANDO NO SE PUEDE PERDER: el
              // server ya lo había borrado de su cola, así que lo devolvemos
              // (sin esto la boca/cara quedaban mudas tras la primera
              // respuesta: el bug que reportó el usuario).
              const detalle = e instanceof Error ? e.message : String(e);
              reportar(
                `diag: no pude escribir "${lineas.join('').trim()}" (${detalle}) -> devuelvo a la cola`
              );
              await bleDevolver(lineas, token);
              await dormir(300);
            }
          }
        } catch (e) {
          if (!vivo) break;
          const msg = e instanceof Error ? e.message : String(e);
          if (msg.includes('otro dispositivo')) {
            // nos robaron el puente: soltamos el server pero NO el enlace
            // (el enlace es del navegador entero y el dueño nuevo lo usa)
            perderPuesto('tx: otro dispositivo lo tomó');
            programarReclamo();
            await dormir(2000);
            continue;
          }
          if (msg.includes('expirado')) {
            // auto-cura: re-registrar y seguir como si nada (y DECIRLO: antes
            // esto era mudo y el bucle podía quedarse girando sin reportar)
            reportar(`diag: el server dio por expirado el puente [${YO}] -> re-registro`);
            if (!(await registrar())) {
              programarReclamo();
              await dormir(2000);
              continue;
            }
            continue;
          }
          // error de red o GATT: seguir intentando, sin matar el latido
          await dormir(500);
        }
      }
    };
    ciclo();
    return () => {
      vivo = false;
    };
  }, [
    token,
    esDueno,
    chars,
    dispositivo,
    ultimaEscrituraMs,
    comandosSinRespuesta,
    reportar,
    registrar,
    perderPuesto,
    programarReclamo,
  ]);

  // ---------- LATIDO EXTRA ----------
  // El long-poll ya late, pero este ping liviano cubre los huecos: mientras
  // reconectamos BLE (sin enlace) o si el long-poll se pausó, el puesto sigue
  // vivo. NO depende del estado de la UI: solo de "quiero ser el puente".
  useEffect(() => {
    let vivo = true;
    const latir = async () => {
      if (!vivo || !esDueno.current) return;
      try {
        await blePing(token);
      } catch (e) {
        if (!vivo) return;
        const msg = e instanceof Error ? e.message : String(e);
        if (msg.includes('expirado')) {
          const ok = await registrar();
          if (!ok) programarReclamo();
        } else if (msg.includes('otro dispositivo')) {
          perderPuesto('ping: otro dispositivo lo tomó');
          programarReclamo();
        }
      }
    };
    const t = setInterval(latir, LATIDO_MS);
    return () => {
      vivo = false;
      clearInterval(t);
    };
  }, [token, esDueno, registrar, perderPuesto, programarReclamo]);
}

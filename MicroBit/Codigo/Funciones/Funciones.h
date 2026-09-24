/**
 * Funciones.h - FUNCIONES de lectura de sensores del micro:bit
 *
 * Este modulo agrupa las herramientas que la IA puede usar (tool calling):
 * cada funcion lee un sensor REAL del micro:bit y devuelve su valor.
 *
 *   Temperatura   -> leerTemperatura()     int  grados Celsius
 *   Luz           -> leerLuz()             int  0..255
 *   Botones       -> leerBotones()         int  bit0=A, bit1=B
 *   Acelerometro  -> leerAcelerometro()    struct (x,y,z,pitch,roll)
 *   Microfono     -> leerMicrofono()       struct (nivel + 5 bandas)
 *   Bateria       -> leerBateria()         struct (mV + fuente)
 *   Gestos        -> leerGesto()           struct (codigo + magnitud)
 *   Brujula       -> leerBrujula()         struct (rumbo + campo + calibracion)
 *   Tactil        -> leerToque()           struct (estado + lectura)
 *
 * Protocolo serial (lo usa el backend):
 *   SENSOR:TEMP  ->  TEMP:24
 *   SENSOR:LUZ   ->  LUZ:120
 *   SENSOR:BOTON ->  BOTON:1:0
 *   SENSOR:ACCEL ->  ACCEL:0:1000:5:0:0
 *   SENSOR:MIC   ->  MIC:nivel:b0:b1:b2:b3:b4:ventanas
 *   SENSOR:BAT   ->  BAT:bateria_mv:vin_mv:fuente
 *   SENSOR:GESTO ->  GESTO:codigo:magnitud_mg
 *   SENSOR:BRUJULA -> BRUJULA:rumbo:campo:calibrada
 *   SENSOR:TOQUE ->  TOUCH:presionado:lectura
 *   CALIBRAR:BRUJULA -> ejecuta la UX manual de CODAL (no es una tool)
 */
#ifndef FUNCIONES_H
#define FUNCIONES_H

#include "Temperatura/Temperatura.h"
#include "Luz/Luz.h"
#include "Botones/Botones.h"
#include "Acelerometro/Acelerometro.h"
#include "Microfono/Microfono.h"
#include "Bateria/Bateria.h"
#include "Gestos/Gestos.h"
#include "Brujula/Brujula.h"
#include "Tactil/Tactil.h"

#endif // FUNCIONES_H

/**
 * Emociones.h - Modulo de EMOCIONES (las caras del micro:bit)
 *
 * CADA emocion vive en su propia carpeta con su animacion compleja:
 *   Alegria/        -> la alegria (Alegria.cpp + Hablar.cpp)  [ya creado]
 *   Triste/         -> la tristeza (Triste.cpp)                [ya creado]
 *   Enojado/        -> el enojo (Enojado.cpp)                  [ya creado]
 *   Sorprendido/    -> la sorpresa (Sorprendido.cpp)           [ya creado]
 *   Neutral/        -> el neutral bruh (Neutral.cpp)            [ya creado]
 *   Fastidio/       -> el fastidio (Fastidio.cpp)               [ya creado]
 *   Miedo/          -> el miedo (Miedo.cpp)                     [ya creado]
 *   Cansado/        -> el cansado (Cansado.cpp)                 [ya creado]
 *   (las demas emociones iran a sus propias carpetas)
 *
 * Este header incluye todas las emociones disponibles.
 */
#ifndef EMOCIONES_H
#define EMOCIONES_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
// (extern = "existe en otro archivo, usala aqui")
extern MicroBit uBit;

// --- Cada emocion en su propia carpeta ---
#include "Alegria/Alegria.h"     // animarAlegria()
#include "Alegria/Hablar.h"      // animarBocaHablando(), modoHablar
#include "Triste/Triste.h"       // animarTriste()
#include "Triste/HablarTriste.h" // animarBocaTriste(), iniciarHablarTriste()
#include "Enojado/Enojado.h"     // animarEnojado()
#include "Enojado/HablarEnojado.h" // animarBocaEnojada(), iniciarHablarEnojado()
#include "Sorprendido/Sorprendido.h"   // animarSorprendido()
#include "Sorprendido/HablarSorprendido.h" // animarBocaSorprendida(), iniciarHablarSorprendido()
#include "Neutral/Neutral.h"   // animarNeutral()
#include "Neutral/HablarNeutral.h" // animarBocaNeutral(), iniciarHablarNeutral()
#include "Fastidio/Fastidio.h"   // animarFastidio()
#include "Fastidio/HablarFastidio.h" // animarBocaFastidio(), iniciarHablarFastidio()
#include "Miedo/Miedo.h"   // animarMiedo()
#include "Miedo/HablarMiedo.h" // animarBocaMiedo(), iniciarHablarMiedo()
#include "Cansado/Cansado.h"   // animarCansado()
#include "Cansado/HablarCansado.h" // animarBocaCansado(), iniciarHablarCansado()

#endif // EMOCIONES_H

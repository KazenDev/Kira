/**
 * SonidoId.h - Los NOMBRES de los sonidos (el catalogo vive en Catalogo.cpp)
 *
 * Esta en el nucleo para que las personalidades puedan decir "este gesto
 * suena asi" sin conocer CODAL. El pegamento (Vida/Sonidos/) los toca.
 */
#ifndef VIDA_SONIDO_ID_H
#define VIDA_SONIDO_ID_H

enum SonidoId
{
    S_NADA = 0,
    // Llegada de cada emocion (mismo orden que EmocionActual)
    S_ENTRADA_ALEGRIA,
    S_ENTRADA_TRISTE,
    S_ENTRADA_ENOJADO,
    S_ENTRADA_SORPRENDIDO,
    S_ENTRADA_NEUTRAL,
    S_ENTRADA_FASTIDIO,
    S_ENTRADA_MIEDO,
    S_ENTRADA_CANSADO,
    // Reacciones (mismo orden que Reaccion, desde R_COSQUILLAS)
    S_COSQUILLAS,
    S_CARINO,
    S_GRUNIDO,
    S_SOBRESALTO,
    S_DESPERTAR,
    S_MAREO,
    S_DORMIR,
    S_TOQUE,
    S_MOLESTA,
    S_BERRINCHE,
    S_OFENDIDA,
    S_ENAMORADA,
    S_PERDON,
    S_ESTORNUDO,
    S_TIRITAR,
    S_ACALORADA,
    S_DESLUMBRADA,
    S_CAIDA,
    // Gestos sueltos
    S_BOSTEZO,
    S_SUSPIRO,
    S_SILBIDO,
    S_UGH,
    S_LAGRIMA,
    S_OH,
    S_HMPH,
    S_TITIRITA,
    S_RISITA,
    S_HOLA,
    // Minijuegos
    S_JUEGO,       // jingle de "¡a jugar!"
    S_TIC,         // el dado rodando
    S_GANA,        // Kira gana / sacaste un 6 / reflejos de rayo
    S_PIERDE,      // Kira pierde / sacaste un 1 / muy lento
    S_DING,        // resultado normal
    S_CUENTA,      // "pie-dra, pa-pel, ti-je-ra"
    S_YA,          // ¡ya! (flash de reflejos, revelar ppt)
    NUM_SONIDOS
};

#endif // VIDA_SONIDO_ID_H

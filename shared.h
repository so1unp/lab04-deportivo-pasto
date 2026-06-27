#ifndef SHARED_H
#define SHARED_H

#include <semaphore.h>

#define FILAS 25
#define COLUMNAS 25
#define MAX_ASTEROIDES 7
#define MAX_NAVES 3

#define MAX_ESTACIONES 1
#define CUPOS_HANGAR 3

// Colas de mensajes POSIX (ver mq_overview(7)). Los nombres empiezan con '/'.
#define COLA_MINERALES "/cosmi_minerales" // muchas naves -> una estacion
#define PAGO_PREFIJO "/cosmi_pago_"       // una cola por nave: se le pega el id

#define MAX_MSGS 10 // capacidad (mensajes) de cada cola

// Cola donde las naves depositan los minerales que traen
typedef struct
{
    int id_nave;
    int cant_mutexio;
    int cant_semaforita;
    int cant_kernelio;
} MensajeMinerales;

// Cola por la que la estacion paga a la nave (oxigeno y combustible)
typedef struct
{
    int id_nave;
    int oxigeno;
    int combustible;
} MensajePago;

typedef struct
{
    int x;
    int y;
    int deuterio;
    int mutexio;
    int semaforita;
    int kernelio;
} Asteroide;

typedef struct
{
    int id;
    int activa;
    int x;
    int y;
    char simbolo;
} Nave;

typedef struct
{
    int id;
    int x;
    int y;
    char simbolo;
    sem_t sem_hangar; // contador de cupos de hangar, inicializado en 3
} Estacion;

typedef struct
{
    Asteroide asteroides[MAX_ASTEROIDES];
    int cantidadAsteroides;
    Nave naves[MAX_NAVES];
    Estacion estaciones[MAX_ESTACIONES];

    sem_t celdas[FILAS][COLUMNAS]; // un semáforo binario por celda del mapa
} Mundo;

#endif
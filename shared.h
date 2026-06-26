#ifndef SHARED_H
#define SHARED_H

#include <semaphore.h>

#define FILAS 25
#define COLUMNAS 25
#define MAX_ASTEROIDES 7
#define MAX_NAVES 3

#define MAX_ESTACIONES 1
#define CUPOS_HANGAR 3

#define COLA_MINERALES 1234
#define PAGO 2605

// Cola donde se deposita los minerales que traen la naves 
typedef struct
{
    long tipo; // obligatorio en System V
    int id_nave;
    int mutexio;
    int semaforita;
    int kernelio;
} MensajeMinerales;

// Cola donde se deposita los pagos que realiza la estacion
typedef struct
{
    long tipo; // obligatorio en System V
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
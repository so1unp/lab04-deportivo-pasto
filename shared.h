#ifndef SHARED_H
#define SHARED_H

#include <semaphore.h>

#define FILAS 25
#define COLUMNAS 25
#define MAX_ASTEROIDES 7
#define MAX_NAVES 3

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
    Asteroide asteroides[MAX_ASTEROIDES];
    int cantidadAsteroides;
    Nave naves[MAX_NAVES];
    sem_t celdas[FILAS][COLUMNAS]; // un semáforo binario por celda del mapa
} Mundo;

#endif
#ifndef SHARED_H
#define SHARED_H

#define FILAS 30
#define COLUMNAS 50
#define MAX_ASTEROIDES 10
#define MAX_NAVES 2

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
} Mundo;

#endif
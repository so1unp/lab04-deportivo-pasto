#ifndef SHARED_H
#define SHARED_H

#define FILAS 30
#define COLUMNAS 50
#define MAX_ASTEROIDES 10

typedef struct {
    int x;
    int y;

    int deuterio;
    int mutexio;
    int semaforita;
    int kernelio;
} Asteroide;

typedef struct {
    Asteroide asteroides[MAX_ASTEROIDES];
    int cantidadAsteroides;
} Mundo;

#endif
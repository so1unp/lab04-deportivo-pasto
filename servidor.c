#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <semaphore.h>
#include "shared.h"

void generarAsteroides(Mundo *mundo)
{
    mundo->cantidadAsteroides = 5;
    srand((unsigned int)time(NULL));
    for (int i = 0; i < mundo->cantidadAsteroides; i++)
    {
        mundo->asteroides[i].x = rand() % COLUMNAS;
        mundo->asteroides[i].y = rand() % FILAS;
        mundo->asteroides[i].deuterio = rand() % 100;
        mundo->asteroides[i].mutexio = rand() % 100;
        mundo->asteroides[i].semaforita = rand() % 100;
        mundo->asteroides[i].kernelio = rand() % 100;
    }
}

void inicializarSemaforos(Mundo *mundo)
{
    for (int y = 0; y < FILAS; y++)
    {
        for (int x = 0; x < COLUMNAS; x++)
        {
            // pshared=1 -> visible entre procesos distintos (no solo hilos)
            // valor inicial=1 -> celda libre
            sem_init(&mundo->celdas[y][x], 1, 1);
        }
    }

    // Las celdas donde hay asteroides arrancan ocupadas (0)
    for (int i = 0; i < mundo->cantidadAsteroides; i++)
    {
        int ax = mundo->asteroides[i].x;
        int ay = mundo->asteroides[i].y;
        sem_trywait(&mundo->celdas[ay][ax]); // 1 -> 0
    }
}

void destruirSemaforos(Mundo *mundo)
{
    for (int y = 0; y < FILAS; y++)
        for (int x = 0; x < COLUMNAS; x++)
            sem_destroy(&mundo->celdas[y][x]);
}

Mundo *mundoGlobal = NULL;
int fdGlobal = -1;

void cerrarServidor(int sig)
{
    (void)sig;
    printf("\nCerrando servidor...\n");
    destruirSemaforos(mundoGlobal);
    munmap(mundoGlobal, sizeof(Mundo));
    close(fdGlobal);
    shm_unlink("/espacio");
    exit(0);
}

int main()
{
    int fd = shm_open("/espacio", O_CREAT | O_RDWR, 0666);
    if (fd == -1) { perror("shm_open"); return 1; }
    if (ftruncate(fd, sizeof(Mundo)) == -1) { perror("ftruncate"); return 1; }

    Mundo *mundo = mmap(NULL, sizeof(Mundo), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mundo == MAP_FAILED) { perror("mmap"); return 1; }

    mundoGlobal = mundo;
    fdGlobal = fd;

    signal(SIGINT, cerrarServidor);

    generarAsteroides(mundo);
    inicializarSemaforos(mundo);

    printf("Servidor iniciado.\n");
    printf("Asteroides generados.\n");
    printf("Semaforos de celdas inicializados.\n");

    while (1)
    {
        sleep(1);
    }

    return 0;
}
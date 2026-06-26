#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <semaphore.h>
#include "shared.h"
#include <sys/msg.h>

void generarAsteroides(Mundo *mundo)
{
    mundo->cantidadAsteroides = MAX_ASTEROIDES;
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
}

void inicializarEstaciones(Mundo *mundo)
{
    for (int i = 0; i < MAX_ESTACIONES; i++)
    {
        mundo->estaciones[i].id = i;
        mundo->estaciones[i].x = 5;
        mundo->estaciones[i].y = 5;
        mundo->estaciones[i].simbolo = 'A';
        sem_init(&mundo->estaciones[i].sem_hangar, 1, CUPOS_HANGAR);
    }
}

void destruirSemaforos(Mundo *mundo)
{
    for (int y = 0; y < FILAS; y++)
        for (int x = 0; x < COLUMNAS; x++)
            sem_destroy(&mundo->celdas[y][x]);

    for (int i = 0; i < MAX_ESTACIONES; i++)
        sem_destroy(&mundo->estaciones[i].sem_hangar);
}

void cerrarServidor(int sig)
{
    (void)sig;
    printf("\nCerrando servidor...\n");
    destruirSemaforos(mundoGlobal);
    munmap(mundoGlobal, sizeof(Mundo));
    close(fdGlobal);
    shm_unlink("/espacio");
    msgctl(colaMinerales, IPC_RMID, NULL);
    msgctl(colaPago, IPC_RMID, NULL);
    exit(0);
}

// variables globales para el servidor
Mundo *mundoGlobal = NULL;
int fdGlobal = -1;

// colas de mensajes globales para el servidor
int colaMinerales;
int colaPago;
int main()
{
    int fd = shm_open("/espacio", O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        perror("shm_open");
        return 1;
    }
    if (ftruncate(fd, sizeof(Mundo)) == -1)
    {
        perror("ftruncate");
        return 1;
    }

    Mundo *mundo = mmap(NULL, sizeof(Mundo), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mundo == MAP_FAILED)
    {
        perror("mmap");
        return 1;
    }

    colaMinerales = msgget(COLA_MINERALES, IPC_CREAT | 0666);
    colaPago = msgget(PAGO, IPC_CREAT | 0666);

    if (colaMinerales == -1 || colaPago == -1)
    {
        perror("msgget");
        exit(1);
    }

    mundoGlobal = mundo;
    fdGlobal = fd;

    signal(SIGINT, cerrarServidor);

    generarAsteroides(mundo);
    inicializarSemaforos(mundo);
    inicializarEstaciones(mundo);

    printf("Servidor iniciado.\n");
    printf("Asteroides generados.\n");
    printf("Semaforos de celdas inicializados.\n");
    printf("Estacion Inicializada.\n");

    while (1)
    {
        sleep(1);
    }

    return 0;
}
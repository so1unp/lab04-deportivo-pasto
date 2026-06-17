#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>

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

Mundo *mundoGlobal = NULL;
int fdGlobal = -1;

void cerrarServidor(int sig)
{
    (void)sig;

    printf("\nCerrando servidor...\n");

    munmap(mundoGlobal, sizeof(Mundo));
    close(fdGlobal);

    shm_unlink("/espacio");

    exit(0);
}

int main()
{
    int fd = shm_open(
        "/espacio",
        O_CREAT | O_RDWR,
        0666);

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

    Mundo *mundo = mmap(
        NULL,
        sizeof(Mundo),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0);

    mundoGlobal = mundo;
    fdGlobal = fd;

    signal(SIGINT, cerrarServidor);

    if (mundo == MAP_FAILED)
    {
        perror("mmap");
        return 1;
    }

    generarAsteroides(mundo);

    printf("Servidor iniciado.\n");
    printf("Asteroides generados.\n");

    while (1)
    {
        sleep(1);
    }

    return 0;
}
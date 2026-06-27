#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <semaphore.h>
#include "shared.h"
#include <mqueue.h>
#include <string.h>

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

// variables globales para el servidor
Mundo *mundoGlobal = NULL;
int fdGlobal = -1;

// Crea (o abre si ya existe) una cola POSIX con capacidad para mensajes de
// 'tam' bytes. Devuelve el descriptor, que cerramos enseguida: la cola
// sobrevive por su nombre hasta que alguien la borre con mq_unlink.
static void crearCola(const char *nombre, long tam)
{
    struct mq_attr attr = {0};
    attr.mq_maxmsg = MAX_MSGS;
    attr.mq_msgsize = tam;

    mqd_t mq = mq_open(nombre, O_CREAT | O_RDWR, 0666, &attr);
    if (mq == (mqd_t)-1)
    {
        perror("mq_open");
        exit(1);
    }
    mq_close(mq);
}

void cerrarServidor(int sig)
{
    (void)sig;
    printf("\nCerrando servidor...\n");
    destruirSemaforos(mundoGlobal);
    munmap(mundoGlobal, sizeof(Mundo));
    close(fdGlobal);
    shm_unlink("/espacio");

    // Borramos las colas POSIX para no dejar recursos huérfanos.
    mq_unlink(COLA_MINERALES);
    for (int i = 0; i < MAX_NAVES; i++)
    {
        char nombre[64];
        snprintf(nombre, sizeof(nombre), "%s%d", PAGO_PREFIJO, i);
        mq_unlink(nombre);
    }
    exit(0);
}

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

    // Cola única donde todas las naves depositan minerales.
    crearCola(COLA_MINERALES, sizeof(MensajeMinerales));

    // Una cola de pago por nave (POSIX no permite filtrar por destinatario,
    // así que cada nave tiene la suya: /cosmi_pago_0, /cosmi_pago_1, ...).
    for (int i = 0; i < MAX_NAVES; i++)
    {
        char nombre[64];
        snprintf(nombre, sizeof(nombre), "%s%d", PAGO_PREFIJO, i);
        crearCola(nombre, sizeof(MensajePago));
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
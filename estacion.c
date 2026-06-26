// estacion.c
#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include "shared.h"

// ── Estructuras ──────────────────────────────────────────────────────────────

// Datos lógicos y de inventario de la estación
typedef struct
{
    int combustible;
    int oxigeno;
    int mineral_mutexio;
    int mineral_semaforita;
    int mineral_kernelio;

    int salir;
    pthread_mutex_t mutex;
} EstadoEstacion;

// ── Variables globales compartidas ───────────────────────────────────────────

EstadoEstacion estacion;
WINDOW *ventana_estacion;
int alto_e, ancho_e;

// ── Hilo: Consumo periódico de combustible de la estación ──────────────────

void *hilo_consumo_estacion()
{
    while (1)
    {
        usleep(6000000); // cada 6 segundos

        pthread_mutex_lock(&estacion.mutex);
        if (estacion.salir)
        {
            pthread_mutex_unlock(&estacion.mutex);
            break;
        }
        if (estacion.combustible > 0)
            estacion.combustible -= 2;
        pthread_mutex_unlock(&estacion.mutex);
    }
    return NULL;
}

void dibujar_panel(int comb, int ox, int mut, int sem, int ker)
{
    werase(ventana_estacion);
    box(ventana_estacion, 0, 0);
    mvwprintw(ventana_estacion, 0, 2, " ESTACION ");

    mvwprintw(ventana_estacion, 2, 2, "Combustible: %3d", comb);
    mvwprintw(ventana_estacion, 3, 2, "Oxigeno:     %3d", ox);

    mvwprintw(ventana_estacion, 5, 2, "Mutexio:     %3d", mut);
    mvwprintw(ventana_estacion, 6, 2, "Semaforita:  %3d", sem);
    mvwprintw(ventana_estacion, 7, 2, "Kernelio:    %3d", ker);

    mvwprintw(ventana_estacion, 9, 2, "'q' para salir");
    wrefresh(ventana_estacion);
}

// ── main ─────────────────────────────────────────────────────────────────────

int main()
{

    int fd = shm_open("/espacio", O_RDWR, 0666);
    if (fd == -1)
    {
        perror("shm_open");
        return 1;
    }

    Mundo *mundo = mmap(NULL, sizeof(Mundo), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mundo == MAP_FAILED)
    {
        perror("mmap");
        return 1;
    }

    // 1. Inicialización de la pantalla ncurses (igual a nave.c)
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    ventana_estacion = newwin(13, 40, 0, 0);

    estacion.combustible = 100;
    estacion.oxigeno = 100;
    estacion.mineral_mutexio = 0;
    estacion.mineral_semaforita = 0;

    estacion.mineral_kernelio = 0;
    estacion.salir = 0;
    pthread_mutex_init(&estacion.mutex, NULL);

    pthread_t tid_consumo;
    pthread_create(&tid_consumo, NULL, hilo_consumo_estacion, NULL);

    int tecla, salir_loop = 0;
    while (!salir_loop)
    {
        tecla = getch();
        if (tecla == 'q')
            salir_loop = 1;

        pthread_mutex_lock(&estacion.mutex);
        int comb = estacion.combustible;
        int ox = estacion.oxigeno;
        int mut = estacion.mineral_mutexio;
        int sem = estacion.mineral_semaforita;
        int ker = estacion.mineral_kernelio;
        pthread_mutex_unlock(&estacion.mutex);

        dibujar_panel(comb, ox, mut, sem, ker);

        usleep(50000);
    }

    pthread_mutex_lock(&estacion.mutex);
    estacion.salir = 1;
    pthread_mutex_unlock(&estacion.mutex);

    pthread_join(tid_consumo, NULL);
    pthread_mutex_destroy(&estacion.mutex);

    munmap(mundo, sizeof(Mundo));
    close(fd);

    endwin();
    return 0;
}
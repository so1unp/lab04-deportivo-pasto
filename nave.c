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

typedef struct
{
    int oxigeno;
    int combustible;
    int salir;
    pthread_mutex_t mutex;
} Recursos;

// ── Variables globales compartidas ───────────────────────────────────────────

Recursos recursos;
WINDOW *ventana;
int alto, ancho;

// ── Hilo: desgaste de oxígeno ──────────────────────────────────

void *hilo_oxigeno()
{
    while (1)
    {
        usleep(5000000);

        pthread_mutex_lock(&recursos.mutex);
        if (recursos.salir) { pthread_mutex_unlock(&recursos.mutex); break; }
        if (recursos.oxigeno > 0) recursos.oxigeno--;
        pthread_mutex_unlock(&recursos.mutex);
    }
    return NULL;
}

// ── Hilo: desgaste de combustible ──────────────────────────────

void *hilo_combustible()
{
    while (1)
    {
        usleep(8000000);

        pthread_mutex_lock(&recursos.mutex);
        if (recursos.salir) { pthread_mutex_unlock(&recursos.mutex); break; }
        if (recursos.combustible > 0) recursos.combustible--;
        pthread_mutex_unlock(&recursos.mutex);
    }
    return NULL;
}

// ── Dibuja la HUD ─────────────────────────────────────────────────────────

void dibujar_hud(int ox, int comb)
{
    mvwprintw(ventana, 1, 2, "OXI: [");
    for (int i = 0; i < 20; i++)
        waddch(ventana, (i < ox / 5) ? '#' : '.');
    wprintw(ventana, "] %3d%%", ox);

    mvwprintw(ventana, 2, 2, "COM: [");
    for (int i = 0; i < 20; i++)
        waddch(ventana, (i < comb / 5) ? '#' : '.');
    wprintw(ventana, "] %3d%%", comb);
}

void dibujarMapa(WINDOW *win, Mundo *mundo)
{
    for (int i = 0; i < mundo->cantidadAsteroides; i++)
    {
        mvwaddch(win, mundo->asteroides[i].y + 5, mundo->asteroides[i].x + 1, 'O');
    }
}

// ── main ─────────────────────────────────────────────────────────────────────

int main()
{
    int fd = shm_open("/espacio", O_RDWR, 0666);
    if (fd == -1) { perror("shm_open"); return 1; }

    Mundo *mundo = mmap(NULL, sizeof(Mundo), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mundo == MAP_FAILED) { perror("mmap"); return 1; }

    // 1. Inicialización ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    getmaxyx(stdscr, alto, ancho);
    ventana = newwin(alto, ancho, 0, 0);

    // 2. Inicializar recursos
    recursos.oxigeno = 100;
    recursos.combustible = 100;
    recursos.salir = 0;
    pthread_mutex_init(&recursos.mutex, NULL);

    // 3. Lanzar hilos
    pthread_t tid_ox, tid_comb;
    pthread_create(&tid_ox, NULL, hilo_oxigeno, NULL);
    pthread_create(&tid_comb, NULL, hilo_combustible, NULL);

    // 4. Crear la nave y reclamar su celda inicial
    int miId = -1;

    for (int i = 0; i < MAX_NAVES; i++)
    {
        if (!mundo->naves[i].activa)
        {
            mundo->naves[i].id = i;
            mundo->naves[i].activa = 1;
            mundo->naves[i].x = COLUMNAS / 2 + i;
            mundo->naves[i].y = FILAS / 2;
            mundo->naves[i].simbolo = 'N';
            miId = i;

            // Reclamamos la celda donde aparecemos (1 -> 0)
            sem_trywait(&mundo->celdas[mundo->naves[i].y][mundo->naves[i].x]);
            break;
        }
    }

    // 5. Loop principal
    int tecla, salir_juego = 0;

    while (!salir_juego)
    {
        werase(ventana);

        tecla = getch();

        pthread_mutex_lock(&recursos.mutex);
        int ox = recursos.oxigeno;
        int comb = recursos.combustible;
        pthread_mutex_unlock(&recursos.mutex);

        int dx = 0, dy = 0;
        int quiereMover = 0;

        switch (tecla)
        {
        case 'w': dy = -1; quiereMover = 1; break;
        case 's': dy =  1; quiereMover = 1; break;
        case 'a': dx = -1; quiereMover = 1; break;
        case 'd': dx =  1; quiereMover = 1; break;
        case 'q':
            // liberamos la celda actual antes de salir
            sem_post(&mundo->celdas[mundo->naves[miId].y][mundo->naves[miId].x]);
            mundo->naves[miId].activa = 0;
            salir_juego = 1;
            break;
        }

        if (quiereMover)
        {
            int nuevoX = mundo->naves[miId].x + dx;
            int nuevoY = mundo->naves[miId].y + dy;

            if (nuevoX >= 0 && nuevoX < COLUMNAS && nuevoY >= 0 && nuevoY < FILAS)
            {
                // Intento no bloqueante de tomar la celda destino
                if (sem_trywait(&mundo->celdas[nuevoY][nuevoX]) == 0)
                {
                    // Conseguida -> liberamos la celda vieja y nos movemos
                    sem_post(&mundo->celdas[mundo->naves[miId].y][mundo->naves[miId].x]);
                    mundo->naves[miId].x = nuevoX;
                    mundo->naves[miId].y = nuevoY;

                    pthread_mutex_lock(&recursos.mutex);
                    if (recursos.combustible > 0)
                        recursos.combustible--;
                    pthread_mutex_unlock(&recursos.mutex);
                }
                // si trywait falla, la celda está ocupada: no nos movemos
            }
        }

        dibujar_hud(ox, comb);
        dibujarMapa(ventana, mundo);

        if (ox == 0 || comb == 0)
        {
            mvwprintw(ventana, alto / 2, ancho / 2 - 10,
                      "*** SIN %s — GAME OVER ***",
                      ox == 0 ? "OXIGENO" : "COMBUSTIBLE");
            wrefresh(ventana);
            usleep(2000000);
            salir_juego = 1;
        }

        for (int i = 0; i < MAX_NAVES; i++)
        {
            if (mundo->naves[i].activa)
            {
                mvwaddch(ventana, mundo->naves[i].y + 5, mundo->naves[i].x + 1, 'N');
            }
        }

        box(ventana, 0, 0);
        wrefresh(ventana);
        usleep(16000);
    }

    pthread_mutex_lock(&recursos.mutex);
    recursos.salir = 1;
    pthread_mutex_unlock(&recursos.mutex);

    pthread_join(tid_ox, NULL);
    pthread_join(tid_comb, NULL);
    pthread_mutex_destroy(&recursos.mutex);

    endwin();
    return 0;
}
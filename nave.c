#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <pthread.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/mman.h>
#include "shared.h" //libreria creada por nosotros donde se guarda la estructura del mapa

// ── Estructuras ──────────────────────────────────────────────────────────────

typedef struct
{
    int x;
    int y;
    chtype simbolo;
} Nave;

typedef struct
{
    int oxigeno;     // 0–100
    int combustible; // 0–100
    int salir;       // flag para terminar los hilos
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
        usleep(5000000); // Tiempo que se gasta el recurso de oxígeno (esta en ms)

        pthread_mutex_lock(&recursos.mutex);
        if (recursos.salir)
        {
            pthread_mutex_unlock(&recursos.mutex);
            break;
        }
        if (recursos.oxigeno > 0)
        {
            recursos.oxigeno--;
        }
        pthread_mutex_unlock(&recursos.mutex);
    }
    return NULL;
}

// ── Hilo: desgaste de combustible ──────────────────────────────

void *hilo_combustible()
{
    while (1)
    {
        usleep(8000000); // Tiempo que se gasta el recurso de combustible (esta en ms)

        pthread_mutex_lock(&recursos.mutex);
        if (recursos.salir)
        {
            pthread_mutex_unlock(&recursos.mutex);
            break;
        }
        if (recursos.combustible > 0)
        {
            recursos.combustible--;
        }
        pthread_mutex_unlock(&recursos.mutex);
    }
    return NULL;
}

// ── Dibuja la HUD (oxígeno y combustible) ────────────────────────────────────

void dibujar_hud(int ox, int comb)
{
    // Barra de oxígeno
    mvwprintw(ventana, 1, 2, "OXI: [");
    for (int i = 0; i < 20; i++)
    {
        if (i < ox / 5)
            waddch(ventana, '#');
        else
            waddch(ventana, '.');
    }
    wprintw(ventana, "] %3d%%", ox);

    // Barra de combustible
    mvwprintw(ventana, 2, 2, "COM: [");
    for (int i = 0; i < 20; i++)
    {
        if (i < comb / 5)
            waddch(ventana, '#');
        else
            waddch(ventana, '.');
    }
    wprintw(ventana, "] %3d%%", comb);
}

void dibujarMapa(WINDOW *ventana, Mundo *mundo)
{
    // Dibujar cada asteroide almacenado
    // en la memoria compartida

    for (int i = 0; i < mundo->cantidadAsteroides; i++)
    {
        mvwaddch(
            ventana,
            mundo->asteroides[i].y + 5,
            mundo->asteroides[i].x + 1,
            'O');
    }
}
// ── main ─────────────────────────────────────────────────────────────────────

int main()
{
    int fd = shm_open(
        "/espacio",
        O_RDWR,
        0666);

    if (fd == -1)
    {
        perror("shm_open");
        return 1;
    }

    Mundo *mundo = mmap(
        NULL,
        sizeof(Mundo),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd, 0);

    if (mundo == MAP_FAILED)
    {
        perror("mmap");
        return 1;
    }

    // 1. Inicialización ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    getmaxyx(stdscr, alto, ancho);
    ventana = newwin(alto, ancho, 0, 0);

    // 2. Inicializar recursos compartidos
    recursos.oxigeno = 100;
    recursos.combustible = 100;
    recursos.salir = 0;
    pthread_mutex_init(&recursos.mutex, NULL);

    // 3. Lanzar hilos
    pthread_t tid_ox, tid_comb;
    pthread_create(&tid_ox, NULL, hilo_oxigeno, NULL);
    pthread_create(&tid_comb, NULL, hilo_combustible, NULL);

    // 4. Crear la nave
    Nave nave;
    nave.x = COLUMNAS / 2;
    nave.y = FILAS / 2;
    nave.simbolo = 'A';

    // 5. Loop principal
    int tecla, salir_juego = 0;

    while (!salir_juego)
    {
        werase(ventana);

        tecla = getch();

        // Leer recursos con lock
        pthread_mutex_lock(&recursos.mutex);
        int ox = recursos.oxigeno;
        int comb = recursos.combustible;
        pthread_mutex_unlock(&recursos.mutex);

        // Movimiento: consume combustible extra al moverse
        int movio = 0;
        switch (tecla)
        {
        case 'w':
            if (nave.y > 0)
            {
                nave.y--;
                movio = 1;
            }
            break;
        case 's':
            if (nave.y < FILAS - 1)
            {
                nave.y++;
                movio = 1;
            }
            break;
        case 'a':
            if (nave.x > 0)
            {
                nave.x--;
                movio = 1;
            }
            break;
        case 'd':
            if (nave.x < COLUMNAS - 1)
            {
                nave.x++;
                movio = 1;
            }
            break;
        case 'q':
            salir_juego = 1;
            break;
        }

        // Cada movimiento gasta 1 unidad extra de combustible
        if (movio)
        {
            pthread_mutex_lock(&recursos.mutex);
            if (recursos.combustible > 0)
                recursos.combustible--;
            pthread_mutex_unlock(&recursos.mutex);
        }

        // Dibujar HUD
        dibujar_hud(ox, comb);

        // Dibujar mapa
        dibujarMapa(ventana, mundo);

        // Mensaje de game over
        if (ox == 0 || comb == 0)
        {
            mvwprintw(ventana, alto / 2, ancho / 2 - 10,
                      "*** SIN %s — GAME OVER ***",
                      ox == 0 ? "OXIGENO" : "COMBUSTIBLE");
            wrefresh(ventana);
            usleep(2000000);
            salir_juego = 1;
        }

        // Dibujar nave
        mvwaddch(
            ventana,
            nave.y + 5,
            nave.x + 1,
            nave.simbolo);

        box(ventana, 0, 0);

        wrefresh(ventana);
        usleep(16000); // ~60 fps
    }

    // 6. Señalar a los hilos que terminen y esperar
    pthread_mutex_lock(&recursos.mutex);
    recursos.salir = 1;
    pthread_mutex_unlock(&recursos.mutex);

    pthread_join(tid_ox, NULL);
    pthread_join(tid_comb, NULL);
    pthread_mutex_destroy(&recursos.mutex);

    // 7. Finalizar
    endwin();
    return 0;
}
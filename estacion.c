// estacion.c
#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <mqueue.h>
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

    int ultimo_pago_nave;  // id de la última nave que depositó (-1 = ninguna aún)
    int ultimo_pago_total; // total de minerales del último depósito

    int salir;
    pthread_mutex_t mutex;
} EstadoEstacion;

// ── Variables globales compartidas ───────────────────────────────────────────

EstadoEstacion estacion;
WINDOW *ventana_estacion;
int alto_e, ancho_e;

mqd_t colaMinerales; // cola de entrada: minerales que traen las naves
                     // la cola de pago es una por nave, se abre on-demand al cobrar

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

// Hilo: cobro de cargamentos  
// Lee los minerales que mandan las naves por COLA_MINERALES, los suma al
// inventario de la estación, calcula cuánto oxígeno/combustible corresponde y
// manda ese pago de vuelta por la cola PAGO.
//
// Usamos lectura NO bloqueante (IPC_NOWAIT) + usleep para que el hilo pueda ver
// el flag 'salir' y terminar limpio cuando se presiona 'q'.

void *hilo_cobro()
{
    MensajeMinerales msg;
    while (1)
    {
        pthread_mutex_lock(&estacion.mutex);
        int salir = estacion.salir;
        pthread_mutex_unlock(&estacion.mutex);
        if (salir)
            break;

        ssize_t r = mq_receive(colaMinerales, (char *)&msg,
                               sizeof(MensajeMinerales), NULL);
        if (r == -1)
        {
            usleep(100000); // todavía no llegó ningún cargamento (EAGAIN)
            continue;
        }

        // Llegó un cargamento: lo sumamos al inventario de la estación.
        int total = msg.cant_mutexio + msg.cant_semaforita + msg.cant_kernelio;

        pthread_mutex_lock(&estacion.mutex);
        estacion.mineral_mutexio += msg.cant_mutexio;
        estacion.mineral_semaforita += msg.cant_semaforita;
        estacion.mineral_kernelio += msg.cant_kernelio;
        estacion.ultimo_pago_nave = msg.id_nave;
        estacion.ultimo_pago_total = total;
        pthread_mutex_unlock(&estacion.mutex);

        // Calculamos el pago.
        // PLACEHOLDER: fórmula a definir (precio justo / precios del config.txt).
        MensajePago pago;
        pago.id_nave = msg.id_nave;
        pago.oxigeno = total * 2;     // PLACEHOLDER
        pago.combustible = total * 2; // PLACEHOLDER

        // POSIX no filtra por destinatario: abrimos la cola propia de ESA nave
        // (/cosmi_pago_<id>) y le mandamos ahí el pago.
        char nombre[64];
        snprintf(nombre, sizeof(nombre), "%s%d", PAGO_PREFIJO, msg.id_nave);
        mqd_t colaPago = mq_open(nombre, O_WRONLY);
        if (colaPago != (mqd_t)-1)
        {
            mq_send(colaPago, (const char *)&pago, sizeof(MensajePago), 0);
            mq_close(colaPago);
        }
    }
    return NULL;
}

void dibujar_panel(int comb, int ox, int mut, int sem, int ker,
                   int ult_nave, int ult_total)
{
    werase(ventana_estacion);
    box(ventana_estacion, 0, 0);
    mvwprintw(ventana_estacion, 0, 2, " ESTACION ");

    mvwprintw(ventana_estacion, 2, 2, "Combustible: %3d", comb);
    mvwprintw(ventana_estacion, 3, 2, "Oxigeno:     %3d", ox);

    mvwprintw(ventana_estacion, 5, 2, "Mutexio:     %3d", mut);
    mvwprintw(ventana_estacion, 6, 2, "Semaforita:  %3d", sem);
    mvwprintw(ventana_estacion, 7, 2, "Kernelio:    %3d", ker);

    if (ult_nave != -1)
        mvwprintw(ventana_estacion, 9, 2, "Ultimo deposito: nave #%d (%d min.)",
                  ult_nave, ult_total);

    mvwprintw(ventana_estacion, 11, 2, "'q' para salir");
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

    // Abrimos la cola de minerales que ya creó el servidor. La leemos sin
    // bloquear (O_NONBLOCK) para que el hilo pueda salir limpio con 'q'.
    colaMinerales = mq_open(COLA_MINERALES, O_RDONLY | O_NONBLOCK);
    if (colaMinerales == (mqd_t)-1)
    {
        perror("mq_open COLA_MINERALES (¿está corriendo el servidor?)");
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
    estacion.ultimo_pago_nave = -1;
    estacion.ultimo_pago_total = 0;
    estacion.salir = 0;
    pthread_mutex_init(&estacion.mutex, NULL);

    pthread_t tid_consumo, tid_cobro;
    pthread_create(&tid_consumo, NULL, hilo_consumo_estacion, NULL);
    pthread_create(&tid_cobro, NULL, hilo_cobro, NULL);

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
        int ult_nave = estacion.ultimo_pago_nave;
        int ult_total = estacion.ultimo_pago_total;
        pthread_mutex_unlock(&estacion.mutex);

        dibujar_panel(comb, ox, mut, sem, ker, ult_nave, ult_total);

        usleep(50000);
    }

    pthread_mutex_lock(&estacion.mutex);
    estacion.salir = 1;
    pthread_mutex_unlock(&estacion.mutex);

    pthread_join(tid_consumo, NULL);
    pthread_join(tid_cobro, NULL);
    pthread_mutex_destroy(&estacion.mutex);

    mq_close(colaMinerales);

    munmap(mundo, sizeof(Mundo));
    close(fd);

    endwin();
    return 0;
}
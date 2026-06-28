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

#define OFFSET_MAPA_Y 7 // filas 1-6 quedan libres para HUD
#define OFFSET_MAPA_X 1

// ── Estructuras ──────────────────────────────────────────────────────────────

typedef struct
{
    int oxigeno;
    int combustible;

    // Inventario propio de la nave (minerales recolectados)
    int deuterio;
    int mutexio;
    int semaforita;
    int kernelio;

    int extrayendo;          // flag del láser (toggle con 'e')
    int id_asteroide_actual; // -1 si no estamos sobre ningún asteroide

    int id_nave;            // id propio de esta nave (para los mensajes)
    int solicitar_deposito; // 1 = el loop pidió depositar minerales en la estación

    int en_hangar; // 1 sii esta, 0 si no

    int salir;
    pthread_mutex_t mutex;
} Recursos;

// ── Variables globales compartidas ───────────────────────────────────────────

Recursos recursos;
Mundo *mundo_global; // puntero a la memoria compartida (lo usa el hilo minero)
WINDOW *ventana;
int alto, ancho;

mqd_t colaMinerales; // cola POSIX donde la nave deposita los minerales 

// ── Hilo: desgaste de oxígeno ──────────────────────────────────

void *hilo_oxigeno()
{
    while (1)
    {
        usleep(5000000);

        pthread_mutex_lock(&recursos.mutex);
        if (recursos.salir)
        {
            pthread_mutex_unlock(&recursos.mutex);
            break;
        }
        if (recursos.oxigeno > 0)
            recursos.oxigeno--;
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
        if (recursos.salir)
        {
            pthread_mutex_unlock(&recursos.mutex);
            break;
        }
        if (recursos.combustible > 0)
            recursos.combustible--;
        pthread_mutex_unlock(&recursos.mutex);
    }
    return NULL;
}

// ── Hilo: extracción de minerales ──────────────────────────────

void *hilo_extraccion()
{
    while (1)
    {
        usleep(500000); // tarda ~0.5s en extraer un bloque

        pthread_mutex_lock(&recursos.mutex);
        if (recursos.salir)
        {
            pthread_mutex_unlock(&recursos.mutex);
            break;
        }

        // Sólo minamos si el láser está encendido, queda combustible y
        // estamos parados sobre un asteroide.
        if (recursos.extrayendo && recursos.combustible > 0 && recursos.id_asteroide_actual != -1)
        {
            int id = recursos.id_asteroide_actual;

            // El semáforo de la celda ya garantiza que somos la única nave
            // sobre este asteroide, así que no hace falta un mutex propio.
            if (mundo_global->asteroides[id].deuterio > 0)
            {
                mundo_global->asteroides[id].deuterio--;
                recursos.deuterio++;
            }
            if (mundo_global->asteroides[id].mutexio > 0)
            {
                mundo_global->asteroides[id].mutexio--;
                recursos.mutexio++;
            }
            if (mundo_global->asteroides[id].semaforita > 0)
            {
                mundo_global->asteroides[id].semaforita--;
                recursos.semaforita++;
            }
            if (mundo_global->asteroides[id].kernelio > 0)
            {
                mundo_global->asteroides[id].kernelio--;
                recursos.kernelio++;
            }

            // Gasta combustible extra por el esfuerzo de minar
            recursos.combustible--;
        }
        pthread_mutex_unlock(&recursos.mutex);
    }
    return NULL;
}

// Hilo: depósito de minerales en la estación 
// Espera a que el loop principal marque 'solicitar_deposito' (solo lo hace si la
// nave está acoplada a una estación). Cuando eso pasa, arma un MensajeMinerales
// con todo el cargamento, lo manda a la cola COLA_MINERALES y vacía la bodega.

void *hilo_deposito()
{
    while (1)
    {
        usleep(100000); // chequea ~10 veces por segundo

        pthread_mutex_lock(&recursos.mutex);
        if (recursos.salir)
        {
            pthread_mutex_unlock(&recursos.mutex);
            break;
        }

        if (!recursos.solicitar_deposito)
        {
            pthread_mutex_unlock(&recursos.mutex);
            continue;
        }

        // Tomamos una foto del cargamento y vaciamos la bodega de inmediato,
        // así no se pierde nada de lo que se siga minando.
        MensajeMinerales msg;
        msg.id_nave = recursos.id_nave;
        msg.cant_mutexio = recursos.mutexio;
        msg.cant_semaforita = recursos.semaforita;
        msg.cant_kernelio = recursos.kernelio;

        recursos.mutexio = 0;
        recursos.semaforita = 0;
        recursos.kernelio = 0;
        recursos.solicitar_deposito = 0;
        pthread_mutex_unlock(&recursos.mutex);

        // Enviamos FUERA del mutex: si la cola estuviera llena, mq_send bloquea
        // y no queremos congelar el resto de los hilos de la nave.
        mq_send(colaMinerales, (const char *)&msg, sizeof(MensajeMinerales), 0);
    }
    return NULL;
}

// ── Hilo: Recepción de pago de la estación ──────────────────
void *hilo_recepcion_pago()
{
    // Construimos el nombre de nuestra cola privada (ej: /cosmi_pago_0)
    char nombre_cola[64];
    snprintf(nombre_cola, sizeof(nombre_cola), "%s%d", PAGO_PREFIJO, recursos.id_nave);
    
    // Abrimos la cola en modo lectura
    mqd_t mi_cola_pago = mq_open(nombre_cola, O_RDONLY);
    if (mi_cola_pago == (mqd_t)-1) {
        return NULL; // Si falla, el hilo muere silenciosamente
    }

    MensajePago pago;
    while (1) {
        // Chequeamos si hay que salir
        pthread_mutex_lock(&recursos.mutex);
        int salir = recursos.salir;
        pthread_mutex_unlock(&recursos.mutex);
        if (salir) break;

        // Esperamos recibir el pago (se bloquea hasta que la estación pague)
        ssize_t r = mq_receive(mi_cola_pago, (char *)&pago, sizeof(MensajePago), NULL);
        
        if (r != -1) {
            // Llegó el pago, actualizamos la nave
            pthread_mutex_lock(&recursos.mutex);
            recursos.oxigeno += pago.oxigeno;
            recursos.combustible += pago.combustible;
            
            // Limitamos a 100 por las dudas
            if(recursos.oxigeno > 100) recursos.oxigeno = 100;
            if(recursos.combustible > 100) recursos.combustible = 100;
            pthread_mutex_unlock(&recursos.mutex);
        }
    }
    mq_close(mi_cola_pago);
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
        mvwaddch(win, mundo->asteroides[i].y + OFFSET_MAPA_Y, mundo->asteroides[i].x + OFFSET_MAPA_X, 'O');
    }

    for (int i = 0; i < MAX_ESTACIONES; i++)
    {
        mvwprintw(win, mundo->estaciones[i].y + OFFSET_MAPA_Y,
                  mundo->estaciones[i].x + OFFSET_MAPA_X, "AAA");
    }
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

    mundo_global = mundo; // el hilo minero accede a la memoria compartida por acá

    // Abrimos la cola de minerales que ya creó el servidor (solo para escribir).
    colaMinerales = mq_open(COLA_MINERALES, O_WRONLY);
    if (colaMinerales == (mqd_t)-1)
    {
        perror("mq_open COLA_MINERALES (¿está corriendo el servidor?)");
        return 1;
    }

    // 1. Inicialización ncurses
    initscr();
    cbreak();
    noecho();

    nodelay(stdscr, TRUE);

    getmaxyx(stdscr, alto, ancho);
    ventana = newwin(alto, ancho, 0, 0);

    // 2. Inicializar recursos
    recursos.oxigeno = 100;
    recursos.combustible = 100;
    recursos.deuterio = 0;
    recursos.mutexio = 0;
    recursos.semaforita = 0;
    recursos.kernelio = 0;
    recursos.extrayendo = 0;           // láser apagado
    recursos.id_asteroide_actual = -1; // no estamos sobre ningún asteroide
    recursos.id_nave = -1;             // se setea al reclamar el slot, más abajo
    recursos.solicitar_deposito = 0;
    recursos.salir = 0;
    pthread_mutex_init(&recursos.mutex, NULL);

    // 3. Lanzar hilos
    pthread_t tid_ox, tid_comb, tid_ext, tid_dep, tid_pago;
    pthread_create(&tid_ox, NULL, hilo_oxigeno, NULL);
    pthread_create(&tid_comb, NULL, hilo_combustible, NULL);
    pthread_create(&tid_ext, NULL, hilo_extraccion, NULL);
    pthread_create(&tid_pago, NULL, hilo_recepcion_pago, NULL);

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

    // Ya sabemos qué nave somos: el hilo de depósito necesita este id para
    // etiquetar sus mensajes, así que lo lanzamos recién ahora.
    recursos.id_nave = miId;
    pthread_create(&tid_dep, NULL, hilo_deposito, NULL);

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
        int quiere_depositar = 0;

        switch (tecla)
        {
        case 'w':
            dy = -1;
            quiereMover = 1;
            break;
        case 's':
            dy = 1;
            quiereMover = 1;
            break;
        case 'a':
            dx = -1;
            quiereMover = 1;
            break;
        case 'd':
            dx = 1;
            quiereMover = 1;
            break;
        case 'e':
            // prende/apaga el láser de extracción
            pthread_mutex_lock(&recursos.mutex);
            recursos.extrayendo = !recursos.extrayendo;
            pthread_mutex_unlock(&recursos.mutex);
            break;
        case 'p':
            // Pedido de depósito; sólo se concreta si estamos acoplados a una
            // estación, cosa que recién sabemos más abajo (estacion_tocada).
            quiere_depositar = 1;
            break;
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

        // ── Detección de colisión con asteroides ──
        int asteroide_tocado = -1;
        for (int i = 0; i < mundo->cantidadAsteroides; i++)
        {
            if (mundo->naves[miId].x == mundo->asteroides[i].x &&
                mundo->naves[miId].y == mundo->asteroides[i].y)
            {
                asteroide_tocado = i;
                break;
            }
        }

        // ──  Deteccion de colisiones con una estacion ──
        int estacion_tocada = -1;
        for (int i = 0; i < MAX_ESTACIONES; i++)
        {
            // Verificamos que la Y coincida exactamente
            // Verificamos que la X de la nave esté entre la X base de la estación y X + 2
            if (mundo->naves[miId].y == mundo->estaciones[i].y &&
                mundo->naves[miId].x >= mundo->estaciones[i].x &&
                mundo->naves[miId].x <= mundo->estaciones[i].x + 2)
            {
                estacion_tocada = i;
                break;
            }
        }

        pthread_mutex_lock(&recursos.mutex);
        
        // ── Lógica del Hangar (Para aprobar los cupos) ──
        if (estacion_tocada != -1 && !recursos.en_hangar) {
            // Intentamos ocupar un slot del hangar (no bloqueante)
            if (sem_trywait(&mundo->estaciones[estacion_tocada].sem_hangar) == 0) {
                recursos.en_hangar = 1; // ¡Entramos!
            } else {
                estacion_tocada = -1; // Hangar lleno, el juego nos ignora como si no estuviéramos
            }
        } else if (estacion_tocada == -1 && recursos.en_hangar) {
            // Nos alejamos de la estación, liberamos el cupo del hangar
            sem_post(&mundo->estaciones[0].sem_hangar); 
            recursos.en_hangar = 0;
        }

        recursos.id_asteroide_actual = asteroide_tocado;
        if (asteroide_tocado == -1) // salimos del asteroide: apagamos el láser
            recursos.extrayendo = 0;
            
        int minando = recursos.extrayendo;
        int inv_d = recursos.deuterio;
        int inv_m = recursos.mutexio;
        int inv_s = recursos.semaforita;
        int inv_k = recursos.kernelio;
        
        // Sólo pedimos depositar si estamos acoplados y hay algo que entregar.
        if (quiere_depositar && estacion_tocada != -1 &&
            (inv_m > 0 || inv_s > 0 || inv_k > 0)) {
            recursos.solicitar_deposito = 1;
        }
            
        pthread_mutex_unlock(&recursos.mutex);

        dibujar_hud(ox, comb);

        if (asteroide_tocado != -1)
        {
            Asteroide *a = &mundo->asteroides[asteroide_tocado];
            mvwprintw(ventana, 5, 2,
                      "ASTEROIDE #%d -> Deu:%d  Mut:%d  Sem:%d  Ker:%d",
                      asteroide_tocado, a->deuterio, a->mutexio, a->semaforita, a->kernelio);
        }

        if (estacion_tocada != -1)
        {
            mvwprintw(ventana, 5, 2, "ESTACION ESPACIAL #%d -> [ACOPLADO]  'p' = depositar minerales", estacion_tocada);
        }

        if (minando)
        {
            wattron(ventana, A_BLINK);
            mvwprintw(ventana, 3, 2, ">>> EXTRACCION ACTIVA <<<");
            wattroff(ventana, A_BLINK);
        }
        mvwprintw(ventana, 4, 2, "INV  Deu:%d  Mut:%d  Sem:%d  Ker:%d",
                  inv_d, inv_m, inv_s, inv_k);

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
                mvwaddch(ventana, mundo->naves[i].y + OFFSET_MAPA_Y, mundo->naves[i].x + OFFSET_MAPA_X, 'N');
            }
        }

        // box(ventana, 0, 0);

        int top = OFFSET_MAPA_Y - 1;
        int left = OFFSET_MAPA_X - 1;
        int bottom = OFFSET_MAPA_Y + FILAS;
        int right = OFFSET_MAPA_X + COLUMNAS;

        /* Horizontal superior e inferior */
        for (int x = left; x <= right; x++)
        {
            mvwaddch(ventana, top, x, '-');
            mvwaddch(ventana, bottom, x, '-');
        }

        /* Vertical izquierda y derecha */
        for (int y = top; y <= bottom; y++)
        {
            mvwaddch(ventana, y, left, '|');
            mvwaddch(ventana, y, right, '|');
        }

        /* Esquinas */
        mvwaddch(ventana, top, left, '+');
        mvwaddch(ventana, top, right, '+');
        mvwaddch(ventana, bottom, left, '+');
        mvwaddch(ventana, bottom, right, '+');

        wrefresh(ventana);
        usleep(16000);
    }

    pthread_mutex_lock(&recursos.mutex);
    recursos.salir = 1;
    pthread_mutex_unlock(&recursos.mutex);

    pthread_join(tid_ox, NULL);
    pthread_join(tid_comb, NULL);
    pthread_join(tid_ext, NULL);
    pthread_join(tid_dep, NULL);
    pthread_mutex_destroy(&recursos.mutex);

    mq_close(colaMinerales);

    endwin();
    return 0;
}
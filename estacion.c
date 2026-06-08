// estacion.c
#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <pthread.h>
#include <unistd.h>

// ── Estructuras ──────────────────────────────────────────────────────────────

// Datos lógicos y de inventario de la estación
typedef struct {
    int combustible;       // 0-100% o unidades globales
    int oxigeno;           // Oxígeno disponible para proveer
    int mineral_mutexio;   // Minerales recolectados/demandados
    int mineral_semaforita;
    int mineral_kernelio;
    int naves_en_hangar;   // Máximo 3 por reglas
    int salir;             // Flag para terminar hilos de soporte
    pthread_mutex_t mutex;
} EstadoEstacion;

// Representación visual en el mockup
typedef struct {
    int x;
    int y;
    chtype simbolo;
} PuntoGrafico;

// ── Variables globales compartidas ───────────────────────────────────────────

EstadoEstacion estacion;
WINDOW *ventana_estacion;
int alto_e, ancho_e;

// ── Hilo: Consumo periódico de combustible de la estación ──────────────────

void *hilo_consumo_estacion() {
    while (1) {
        // La estación consume combustible de mantenimiento de fondo de forma periódica
        usleep(6000000); // Cada 6 segundos

        pthread_mutex_lock(&estacion.mutex);
        if (estacion.salir) {
            pthread_mutex_unlock(&estacion.mutex);
            break;
        }
        
        if (estacion.combustible > 0) {
            estacion.combustible -= 2; // Consumo de soporte vital del hangar
        }
        pthread_mutex_unlock(&estacion.mutex);
    }
    return NULL;
}

// ── Dibuja el panel de control e inventario (HUD) ───────────────────────────

void dibujar_hud_estacion(int comb, int ox, int naves, int m_mut, int m_sem, int m_ker) {
    // Estado crítico de la estación
    mvwprintw(ventana_estacion, 1, 2, "ESTACION DE REABASTECIMIENTO SECTOR ACTIVO");
    mvwprintw(ventana_estacion, 2, 2, "COMBUSTIBLE ENERGIA: [%3d%%] ", comb);
    mvwprintw(ventana_estacion, 3, 2, "RESERVA DE OXIGENO:  [%3d%%] ", ox);
    
    // Ocupación de hangares (Semáforo contador lógico máximo 3)
    mvwprintw(ventana_estacion, 4, 2, "HANGAR DE ANCLAJE:   [%d / 3 NAVES]", naves);
    if (naves >= 3) {
        wattron(ventana_estacion, A_BLINK);
        mvwprintw(ventana_estacion, 4, 30, "⚠️ HANGAR LLENO");
        wattroff(ventana_estacion, A_BLINK);
    }

    // Depósito de minerales demandados
    mvwprintw(ventana_estacion, 6, 2, "DEPOSITO DE MINERALES ALMACENADOS:");
    mvwprintw(ventana_estacion, 7, 4, "-> Mutexio:    [%d unidades]", m_mut);
    mvwprintw(ventana_estacion, 8, 4, "-> Semaforita: [%d unidades]", m_sem);
    mvwprintw(ventana_estacion, 9, 4, "-> Kernelio:   [%d unidades]", m_ker);

    if (comb < 20) {
        wattron(ventana_estacion, A_REVERSE);
        mvwprintw(ventana_estacion, 11, 2, " ALERTA: SOLICITANDO DEUTERIO AL CUADRANTE ");
        wattroff(ventana_estacion, A_REVERSE);
    }
}

// ── Dibuja la estructura física del hangar en base a tu mockup ──────────────

void dibujar_mockup_hangar() {
    int centro_y = alto_e / 2 + 3;
    int centro_x = ancho_e / 2 + 10;

    // Pasillo Superior (Hangar A)
    mvwaddch(ventana_estacion, centro_y - 3, centro_x - 4, '|');
    mvwprintw(ventana_estacion, centro_y - 3, centro_x - 2, " ");
    mvwaddch(ventana_estacion, centro_y - 3, centro_x,     '|');
    mvwaddch(ventana_estacion, centro_y - 2, centro_x - 4, '|');
    mvwaddch(ventana_estacion, centro_y - 2, centro_x,     '|');

    // Pasillo Inferior (Hangar C)
    mvwaddch(ventana_estacion, centro_y + 2, centro_x - 4, '|');
    mvwaddch(ventana_estacion, centro_y + 2, centro_x,     '|');
    mvwaddch(ventana_estacion, centro_y + 3, centro_x - 4, '|');
    mvwprintw(ventana_estacion, centro_y + 3, centro_x - 2, " ");
    mvwaddch(ventana_estacion, centro_y + 3, centro_x,     '|');

    // Estructura Horizontal y Pasillo Derecho (Hangar B)
    for(int i = -15; i < -4; i++) {
        mvwaddch(ventana_estacion, centro_y - 1, centro_x + i, '_');
        mvwaddch(ventana_estacion, centro_y + 1, centro_x + i, '_');
    }
    for(int i = 1; i < 10; i++) {
        mvwaddch(ventana_estacion, centro_y - 1, centro_x + i, '_');
        mvwaddch(ventana_estacion, centro_y + 1, centro_x + i, '_');
    }
    
    // Entrada del callejón B
    mvwaddch(ventana_estacion, centro_y - 1, centro_x + 10, '|');
    mvwprintw(ventana_estacion, centro_y,     centro_x + 12, " ");
    mvwaddch(ventana_estacion, centro_y + 1, centro_x + 10, '|');
    
    // Fondo izquierdo ciego
    mvwaddch(ventana_estacion, centro_y, centro_x - 15, '|');
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    // 1. Inicialización de la pantalla ncurses (igual a nave.c)
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    getmaxyx(stdscr, alto_e, ancho_e);
    ventana_estacion = newwin(alto_e, ancho_e, 0, 0);

    // 2. Inicializar el estado de la estación
    estacion.combustible       = 100; // Estado inicial lleno
    estacion.oxigeno           = 100;
    estacion.mineral_mutexio   = 12;  // Reservas iniciales base
    estacion.mineral_semaforita = 8;
    estacion.mineral_kernelio   = 4;
    estacion.naves_en_hangar   = 0;   // Comienza libre sin naves
    estacion.salir             = 0;
    pthread_mutex_init(&estacion.mutex, NULL);

    // 3. Lanzar hilo de simulación de consumo energético
    pthread_t tid_consumo;
    pthread_create(&tid_consumo, NULL, hilo_consumo_estacion, NULL);

    // 4. Bucle principal de refresco gráfico interactivo
    int tecla, salir_estacion = 0;

    while (!salir_estacion) {
        werase(ventana_estacion);

        tecla = getch();

        // Control manual de simulación para pruebas del parcial
        switch (tecla) {
            case '1': // Simula que ingresa una nave al hangar
                pthread_mutex_lock(&estacion.mutex);
                if (estacion.naves_en_hangar < 3) estacion.naves_en_hangar++;
                pthread_mutex_unlock(&estacion.mutex);
                break;
            case '2': // Simula que se va una nave del hangar
                pthread_mutex_lock(&estacion.mutex);
                if (estacion.naves_en_hangar > 0) estacion.naves_en_hangar--;
                pthread_mutex_unlock(&estacion.mutex);
                break;
            case 'r': // Simula recepción/compra de cargamento minero
                pthread_mutex_lock(&estacion.mutex);
                estacion.mineral_mutexio += 5;
                pthread_mutex_unlock(&estacion.mutex);
                break;
            case 'q': // Salida limpia
                salir_estacion = 1;
                break;
        }

        // Leer variables de estado de forma segura con lock para dibujar el HUD
        pthread_mutex_lock(&estacion.mutex);
        int comb  = estacion.combustible;
        int ox    = estacion.oxigeno;
        int naves = estacion.naves_en_hangar;
        int m_mut = estacion.mineral_mutexio;
        int m_sem = estacion.mineral_semaforita;
        int m_ker = estacion.mineral_kernelio;
        pthread_mutex_unlock(&estacion.mutex);

        // Control de Game Over catastrófico: Si la estación se queda sin combustible el juego termina
        if (comb == 0) {
            mvwprintw(ventana_estacion, alto_e / 2, ancho_e / 2 - 25, 
                      "*** ENERGIA AGOTADA — CATASTROFE DEL SECTOR EN EL HANGAR ***");
            wrefresh(ventana_estacion);
            usleep(3000000); // Pausa de 3 segundos
            salir_estacion = 1;
        }

        // Dibujar componentes en pantalla
        dibujar_hud_estacion(comb, ox, naves, m_mut, m_sem, m_ker);
        dibujar_mockup_hangar();
        box(ventana_estacion, 0, 0);

        wrefresh(ventana_estacion);
        usleep(16000); // Sincronizado a ~60 fps como la nave
    }

    // 5. Apagado controlado de hilos y destrucción de mutex (Evita recursos huérfanos)
    pthread_mutex_lock(&estacion.mutex);
    estacion.salir = 1;
    pthread_mutex_unlock(&estacion.mutex);

    pthread_join(tid_consumo, NULL);
    pthread_mutex_destroy(&estacion.mutex);

    endwin();
    return 0;
}
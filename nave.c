#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>

// Estructura para representar la nave
typedef struct {
    int x;
    int y;
    chtype simbolo;
} Nave;

int main() {
    // 1. Inicialización estándar
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);  // getch() no bloquea

    int alto, ancho;
    getmaxyx(stdscr, alto, ancho);  // Obtiene el tamaño de la terminal
    int ventana_y = 0, ventana_x = 0;
    WINDOW *ventana = newwin(alto, ancho, ventana_y, ventana_x);
    
    // 3. Crear la nave
    Nave nave;
    nave.x = 10;
    nave.y = 5;
    nave.simbolo = 'A';
    
    
    // 4. Variables para el loop
    int tecla;
    int salir = 0;
    
    // 5. Loop principal del juego
    while (!salir) {
        // Limpiar la ventana
        werase(ventana);
        
        // Capturar entrada
        tecla = getch();
        
        // Procesar movimiento
        switch (tecla) {
            case 'w':  // Arriba
                if (nave.y > 0) {
                    nave.y--;
                }
                break;
            case 's':  // Abajo
                if (nave.y < alto - 1) {
                    nave.y++;
                }
                break;
            case 'a':  // Izquierda
                if (nave.x > 0) {
                    nave.x--;
                }
                break;
            case 'd':  // Derecha
                if (nave.x < ancho - 1) {
                    nave.x++;
                }
                break;
            case 'q':  // Salir
                salir = 1;
                break;
        }
        
        // Dibujar la nave
        mvwaddch(ventana, nave.y, nave.x, nave.simbolo);
        
        // Dibujar bordes
        box(ventana, 0, 0);
        
        // Refrescar
        wrefresh(ventana);
    }
    
    // 6. Finalizar
    endwin();
    return 0;
}
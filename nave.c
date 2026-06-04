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

    // 2. Crear ventana
    int alto = 10, ancho = 30, y = 7, x = 50;
    WINDOW *ventana = newwin(alto, ancho, y, x);
    
    // 3. Crear la nave
    Nave nave;
    nave.x = 5;          // Posición X dentro de la ventana
    nave.y = 2;          // Posición Y dentro de la ventana
    nave.simbolo = 'A';  // Carácter que representa la nave
    
    // 4. Dibujar la nave en la ventana
    mvwaddch(ventana, nave.y, nave.x, nave.simbolo);
    
    // 5. Refrescar la pantalla base y la ventana
    refresh();           // Refresca la pantalla principal
    wrefresh(ventana);   // Refresca tu ventana específica

    // 6. Esperar entrada
    getch();
    
    // 7. Finalizar
    endwin();
    return 0;
}
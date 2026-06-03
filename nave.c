#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>

int main() {
    // 1. Inicialización estándar
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    // 2. Crear ventana
    int alto = 10, ancho = 30, y = 7, x = 50;
    WINDOW *ventana = newwin(alto, ancho, y, x);
    
    // 4. IMPORTANTE: Refrescar la pantalla base y la ventana
    refresh();          // Refresca la pantalla principal
    wrefresh(ventana);  // Refresca tu ventana específica

    // 5. Esperar entrada (esto debería pausar el programa)
    getch();
    // 6. Finalizar
    endwin();
    return 0;
}
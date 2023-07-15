/*** includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/
struct termios orig_termios;

/*** terminal ***/
// Manejo de errores
void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode () {
    if (tcsetattr(STDIN_FILENO, TCSADFLUSH, &orig_termios) == -1)
        die("tcsetattr"); // Indica la falla
}

/**
 * ICRNL Tiene activa la combinacion Ctrl M que provoca que no se lea correctamente
 * IXON permite que Ctrl S y Ctrl Q que son usadas para el control de flujo, el primero detiene la transmision de datos hasta presionar el segundo
 * OPOST regularmente un salto de linea viene acompañado de un retorno de carro, se desctiva y al dar enter no regresa al inicio de la linea
 * ECHO hace que cada tecla que se escriba se imprima en terminal, se desactiva y no muestra la tecla
 * ICANON permite leer linea por linea en la entrada, se desactiva y permite leerla byte por byte 
 * IEXTEN cuando se presiona Ctrl V se espera que se escriba un caracter y envia dicho caracter
 * ISIG permite que Ctrl C envia señal para finalizar proceso y Ctrl Z envia señal para suspender procesos se decativa para que no realice dicha acción
 * BRKINT, INPCK, ISTRIP, CS8 No tienen efecto sobre lo que se puede ver, pero tradicionalmente se considera su desactivacion como parte del "modo sin procesar"
 * CS8 establece el tamaño de caracteres a 8 bits
 */

void enableRawMode () {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcgetattr"); // Indica la falla
    
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcgetattr"); // Indica la falla
}

/*** init ***/
int main () {
    enableRawMode();

    char c;
    
    do {
        c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) 
            die("read"); // Indica la falla
        // imprime caracteres, del 0 - 31 son de control y solo imprime el número
        iscntrl(c) ? printf("%d\r\n", c) : printf("%d ('%c')\r\n", c, c);
    }
    while (c != CTRL_KEY('q'));
    
    return 0;
}

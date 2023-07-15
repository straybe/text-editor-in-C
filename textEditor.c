/*** includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
#define UwU_TEXT_EDITOR_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,       
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
};

/*** data ***/
struct editorConfig {
    int cx;
    int cy;
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/
// Manejo de errores
void die(const char *s) {
    write(STDOUT_FILENO, "x1b[2J", 4);
    write(STDOUT_FILENO, "x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode () {
    if (tcsetattr(STDIN_FILENO, TCSADFLUSH, &E.orig_termios) == -1)
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
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr"); // Indica la falla
    
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcgetattr"); // Indica la falla
}

/**
 * Espera una pulsacion de tecla para devolverla. 
 * 
 * Relaciona los casos de los valores del codigo ascci que son de control, los cuales corresponden a los primeros 25
 * 
 */
int editorReadKey () {
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read"); 
    }
    
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return;

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }
            else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        
        return '\x1b';
    }
    else   
        return c;
}

int getCursorPosition (int *rows, int *cols) {

    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) -1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }

    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

/**
 * @brief Obtiene el tamaño de la ventana y las coloca el la estructura winsize
 * colocando el cursor en la esquina inferior derecha para que funcione en cuaquier SO
 * La letra C hace referencia a las comunas que se deben desplazar y B a las filas, 
 * se coloca un valor de 999 para alcanzar la esquina inferior derecha
 * 
 * @param rows 
 * @param cols 
 * @return int
 */
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12){
            return -1;
        }
        editorReadKey();
        return getCursorPosition(rows, cols);
    }
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** append buffer ***/
// Declaracion de un buffer, puntero de la cadena en memoria y el tamaño
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend (struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;    
}

void abFree (struct abuf *ab) {
    free(ab->b);
}

/*** output ***/

/**
 * Se encarga de dibujar cada fila del bufer de texto que se esta editando 
 */
void editorDrawRows (struct abuf *ab) {
    char buffer[10];

    for (int y = 0; y < E.screenrows; y++) {

        if (y == E.screenrows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome), "UwU_Text_Editor --version %s", UwU_TEXT_EDITOR_VERSION);

            if (welcomelen > E.screencols) welcomelen = E.screencols;

            int padding = (E.screencols - welcomelen) / 2;
            if (padding) {
                sprintf(buffer, "%d", (y+1));
                abAppend(ab, buffer, strlen(buffer));
                padding--;
            }

            while (padding--) abAppend(ab, " ", 1);
            
            abAppend(ab, welcome, welcomelen);
        }
        else {
            sprintf(buffer, "%d", (y+1));
            abAppend(ab, buffer, strlen(buffer));
        }

        abAppend(ab, "\x1b[K", 3);

        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

/**
 * El 4 en write significan 4 bytes en la terminal, el primero es \xb1 que es un caracter de escape
 * los otros 3 bytes son [2J. Con [ se comienza el caracter de escape, J es el comando para borrar la pantalla
 * el 2 dice que el se borre toda la pantalla, 1 barra la pantalla hasta donde esta el cursor y 0 borra 
 * despues del cursor hasta el final de la pantalla.
 * La H es para el posicionamiento del cursor
 */
void editorRefreshScreen () {
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input ***/

/**
 * @brief Se encarga del movimiento de en la pantalla, tabien evita que elos valores sean negativos
 * 
 * 
 * @param key 
 */
void editorMoveCursor (int key) {
    switch (key) {
    case ARROW_LEFT:
        if (E.cx != 0){
            E.cx--;
        }
        break;
    case ARROW_RIGHT:
        if (E.cx != E.screencols - 1) {
            E.cx++;
        }
        break;
    case ARROW_UP:
        if (E.cy != 0) {
            E.cy--;
        }
        break;
    case ARROW_DOWN:
        if (E.cy != E.screenrows - 1) {
            E.cy++;
        }
        break;
    }
}

/**
 * Espera por cada tecla presionada para insertar cualquier carater alfanumerico,
 * asi como teclas imprimibles o combinaciones de las mismas
*/
void editorProcessKeypress () {
    int c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "x1b[2J", 4);
            write(STDOUT_FILENO, "x1b[H", 3);
            exit(0);
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            E.cx = E.screencols - 1;
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            int times = E.screenrows;
            while (times--) editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}

/*** init ***/

/**
 * @brief establece el tamaño de la ventana si no hay un error
*/
void initEditor () {
    E.cx = 0;
    E.cy = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowsSize");
}

int main () {
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}

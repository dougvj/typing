
/****************************************************************************
   (C) 2012 Doug Johnson


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>

// This function draws the buffer at a given position (start) and at an offset.
// from the center. Rows and Columns are the center of the screen. This function
// also takes any ncurses attribute flags to add to the buffer output
void drawBuffer(char *buffer, int start, int offset, int rows, int columns,
                int attribute) {
  move((rows / 2) + offset, 0);
  attron(attribute);
  for (int i = start; i < columns + start && buffer[i] != 0; i++)
    addch(buffer[i]);
  attroff(attribute);
  int y, x;
  getyx(stdscr, y, x);
  printw("   ");
  move(y, x);
}

// This draws the input buffer, it wraps the above function to draw bolded white
// text
void drawCurrentBuffer(char *buffer, int start, int offset, int rows,
                       int columns) {
  drawBuffer(buffer, start, offset, rows, columns, A_BOLD | COLOR_PAIR(1));
}

// This function draws the current input buffer in blue. See above for more
// details
void drawCurrentInput(char *input, int start, int offset, int rows,
                      int columns) {
  drawBuffer(input, start, offset, rows, columns, COLOR_PAIR(3));
}

void drawCursor() {
  // Here we draw the cursor
  attron(COLOR_PAIR(3) | A_BOLD);
  addch('_');
  attroff(COLOR_PAIR(3) | A_BOLD);
}

// This function adds a character to the input buffer output and paints it
// either blue or red depending on whether wrong is set correctly. The offset is
// row offset from center (same as above). Pos is the column offset to draw it
// at. As always to calculate the proper distance from center, the number of
// rows and columns are needed
void addCharacter(int wrong, char chr, int offset, int pos, int rows,
                  int columns) {
  move((rows / 2) + offset, pos);
  if (wrong) {
    attron(COLOR_PAIR(2));
    addch(chr);
    attroff(COLOR_PAIR(2));
  } else {
    attron(COLOR_PAIR(3));
    addch(chr);
    attroff(COLOR_PAIR(3));
    drawCursor();
  }
}

// This function initialized the ncurses library. It enables color, sets up the
// color pairs, turns off echoing and etc.
void initCurses() {
  initscr();
  start_color();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  init_pair(2, COLOR_BLACK, COLOR_RED);
  init_pair(1, COLOR_WHITE, COLOR_BLACK);
  init_pair(3, COLOR_BLUE, COLOR_BLACK);
  init_pair(4, COLOR_WHITE, COLOR_BLUE);
  curs_set(0);
}

// This function takes the buffer and input with relevant position data and red-
// raws the screen. This is useful for when the screen size changes or for when
// the text must scroll off the screen
void redraw(char *buffer, char *input, int spos, int cpos, int rows,
            int columns) {
  drawCurrentBuffer(buffer, spos, -1, rows, columns);
  drawCurrentInput(input, spos, 0, rows, columns);
  drawCursor();
  refresh();
}

// This handy function returns the current unix time as a double for higher
// precision into the microsecond range.
double get_time() {
  struct timeval t;
  struct timezone tzp;
  gettimeofday(&t, &tzp);
  return t.tv_sec + t.tv_usec * 1e-6;
}

typedef struct {
  double start_time;
  int chr, wd, incor;
} TypingStats;

// This function prints real time statistics on the bottom of the page
void print_stats(TypingStats *stats) {
  double time = get_time() - stats->start_time;
  int chr = stats->chr;
  int incor = stats->incor;
  int wd = (chr - incor) / 5;
  attron(COLOR_PAIR(4));
  printw("apm: %.3f wpm: %.3f accuracy: %2.1f",
         chr / time * 60, wd / time * 60,
         100 * (chr - incor) / (float)chr);
  attroff(COLOR_PAIR(4));
}

void print_final_stats(TypingStats *stats) {
  double time = get_time() - stats->start_time;
  int chr = stats->chr;
  int incor = stats->incor;
  int wd = (chr - incor) / 5;
  printf("actions per minute: %.3f\n"
         "words per minute: %.3f\n"
         "accuracy: %2.1f\n",
         chr / time * 60,
         wd / time * 60,
         100.0 * (chr - incor) / (float)chr);

}

void quit_error(char *err) {
  endwin();
  fprintf(stderr, "%s", err);
  exit(0);
}

TypingStats* _current_stats = NULL;

void handle_signal(int sig) {
  endwin();
  if (_current_stats)  {
    print_final_stats(_current_stats);
  }
  exit(0);
}

// This function contains all of the logic for the system. It takes  a file
// descriptor and reads that into a buffer and sets up the screen and
// scrolling logic. When the user is done typing, it outputs final statistics
// outside of ncurses mode
void typing(FILE *desc) {
  // Initialized ncurses
  initCurses();
  char *buffer;
  char *input;
  if (desc == NULL) {
    quit_error("File descriptor invalid. Did you give a valid filename?\n");
  }
  // Find out how large our file is
  fseek(desc, 0L, SEEK_END);
  int fsize = ftell(desc);
  fseek(desc, 0L, SEEK_SET);
  // Allocate and initialize our buffers (plus 1 for null termination)
  buffer = malloc(fsize + 1);
  input = malloc(fsize + 1);
  memset(input, 0, fsize + 1);
  memset(buffer, 0, fsize + 1);
  int ch = 10; // cannot be initialized to 0 because that is EOF
  int pos;
  // Read in characters skipping any line or tab characters
  // TODO sanitize for ASCII or valid typable UTF characters only.
  for (pos = 0; ch != EOF; ch = getc(desc), pos++) {
    if (ch == '\n' || ch == '\t' || ch == '\r') {
      pos--;
      continue;
    }
    buffer[pos] = ch;
  }
  // Set our variables
  pos = 0;
  int spos = 0;
  int cpos = 0;
  int rows, columns;
  TypingStats stats = {};
  // get current window size
  getmaxyx(stdscr, rows, columns);
  // Draw our window for the first time
  redraw(buffer, input, pos, spos, rows, columns);
  // Check if there is valid stuff
  if (buffer[pos] == 0) {
    quit_error("Given file is empty or invalid\n");
  }
  // While we are not at the end of the buffer keep going
  while (buffer[pos] != 0) {
    // Wait for character press
    ch = getch();
    // If this is a resize event, then update the screen
    // TODO add a clear screen routine
    if (ch == KEY_RESIZE) {
      getmaxyx(stdscr, rows, columns);
      redraw(buffer, input, spos, cpos, rows, columns);
      continue;
    }
    // If this is the first keypress, record the time
    // and the current stats
    if (stats.start_time == 0) {
      stats.start_time = get_time();
      _current_stats = &stats;
    }
    // If this is the enter key, treat as space
    if (ch == '\n')
      ch = ' ';
    // Record the input stroke
    input[pos] = ch;
    // Determine if the keypress was correct or not
    int wrong = (input[pos] != buffer[pos]);
    // Add character to output
    addCharacter(wrong, ch, 0, cpos, rows, columns);
    // printw("'%c' 0x%X\n", ch, ch);
    // Advance if we are correct, otherwise mark incorrect this stroke for
    // statistical purposes
    if (!wrong) {
      pos++;
      cpos++;
    } else {
      stats.incor++;
    }
    // If we have reached the middle of the screen, then we need to redraw
    // the screen with different offsets and keep the cursor in the middle.
    if (cpos > columns / 2) {
      spos += cpos - (columns / 2);
      cpos = columns / 2;
      redraw(buffer, input, spos, cpos, rows, columns);
    }
    // Mark keystroke count
    stats.chr++;
    // Print the statistics on the bottom and restor the cursor after
    move(rows - 2, 0);
    print_stats(&stats);
    printw("\npos: %i spos: %i cpos: %i fsize: %i", pos, spos, cpos, fsize);
    move(rows / 2, spos);
    // redraw the screen after every stroke
    refresh();
  }
  // End the program and print the statistics
  endwin();
  print_final_stats(&stats);
  _current_stats = NULL;
}

int main(int argc, char **argv) {
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);
  if (argc >= 2) {
    FILE *input = fopen(argv[1], "r");
    typing(input);
    fclose(input);
  } else {
    fprintf(stderr, "Invalid arguments\n");
    return 0;
  }
  return 1;
}

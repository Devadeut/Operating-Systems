#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "boardgen.c"

void genblocks ( int B[9][9], int bfd[9][2] )
{
   int b, i, j;
   char bno[8], bfdin[16], bfdout[16], r1fd[16], r2fd[16], c1fd[16], c2fd[16], geometry[64], title[16];

   for (b=0; b<9; ++b) pipe(bfd[b]);
   for (b=0; b<9; ++b) {
      i = b / 3; j = b % 3;
      sprintf(bno, "%d", b);
      sprintf(bfdin, "%d", bfd[b][0]);
      sprintf(bfdout, "%d", bfd[b][1]);
      if (b % 3 == 0) {
         sprintf(r1fd, "%d", bfd[b+1][1]);
         sprintf(r2fd, "%d", bfd[b+2][1]);
      } else if (b % 3 == 1) {
         sprintf(r1fd, "%d", bfd[b-1][1]);
         sprintf(r2fd, "%d", bfd[b+1][1]);
      } else {
         sprintf(r1fd, "%d", bfd[b-2][1]);
         sprintf(r2fd, "%d", bfd[b-1][1]);
      }
      if (b < 3) {
         sprintf(c1fd, "%d", bfd[b+3][1]);
         sprintf(c2fd, "%d", bfd[b+6][1]);
      } else if (b < 6) {
         sprintf(c1fd, "%d", bfd[b-3][1]);
         sprintf(c2fd, "%d", bfd[b+3][1]);
      } else {
         sprintf(c1fd, "%d", bfd[b-6][1]);
         sprintf(c2fd, "%d", bfd[b-3][1]);
      }
      sprintf(geometry, "17x8+%d+%d", 800+240*j, 300+260*i);
      sprintf(title, "Block %d", b);
      if (!fork()) {
         execlp("xterm", "xterm",
                "-T", title,
                "-fa", "Monospace", "-fs", "15",
                "-geometry", geometry, "-bg", "#331100",
                "-e", "./block", bno, bfdin, bfdout, r1fd, r2fd, c1fd, c2fd, NULL);
      }
   }
}

void printhelp ( )
{
   printf("\nCommands supported\n");
   printf("\tn\t\tStart new gane\n");
   printf("\tp b c d\t\tPut digit d [1-9] at cell c [0-8] of block b [0-8]\n");
   printf("\ts\t\tShow solution\n");
   printf("\th\t\tPrint this help message\n");
   printf("\tq\t\tQuit\n");
   printf("\nNumbering scheme for blocks and cells\n");
   printf("\t+---+---+---+\n");
   printf("\t| 0 | 1 | 2 |\n");
   printf("\t+---+---+---+\n");
   printf("\t| 3 | 4 | 5 |\n");
   printf("\t+---+---+---+\n");
   printf("\t| 6 | 7 | 8 |\n");
   printf("\t+---+---+---+\n\n");
}

void initboard ( int B[9][9], int bfd[9][2], int dupout )
{
   int b, i, j;

   for (i=0; i<3; ++i) {
      for (j=0; j<3; ++j) {
         b = 3*i + j;
         close(1);
         dup(bfd[b][1]);
         printf("N %d %d %d %d %d %d %d %d %d\n",
            B[3*i][3*j], B[3*i][3*j+1], B[3*i][3*j+2],
            B[3*i+1][3*j], B[3*i+1][3*j+1], B[3*i+1][3*j+2],
            B[3*i+2][3*j], B[3*i+2][3*j+1], B[3*i+2][3*j+2]);
         close(1);
         dup(dupout);
      }
   }
}

void tryset ( int bfd[9][2], int dupout )
{
   int b, c, d;

   scanf("%d%d%d", &b, &c, &d);
   if ((b < 0) || (b > 8) || (c < 0) || (c > 8) || (d < 1) || (d > 9)) {
      printf("Invalid request\n");
      return;
   }
   close(1);
   dup(bfd[b][1]);
   printf("P %d %d\n", c, d);
   close(1);
   dup(dupout);
}

void endgame ( int bfd[9][2], int dupout )
{
   int b;

   for (b=0; b<9; ++b) {
      close(1);
      dup(bfd[b][1]);
      printf("Q\n");
   }
   close(1);
   dup(dupout);
   for (b=0; b<9; ++b) wait(NULL);
   printf("Bye...\n");
   exit(0);
}

int main ( )
{
   int B[9][9], S[9][9];
   int bfd[9][2];
   int dupout;
   char cmd;

   genblocks(B,bfd);
   dupout = dup(1);

   printhelp();
   while (1) {
      printf("Foodoku> ");
      scanf("%c", &cmd);
      switch (cmd) {
         case 'h':
         case 'H':
            printhelp();
            break;
         case 'n':
         case 'N':
            newboard(B,S);
            initboard(B,bfd,dupout);
            break;
         case 'p':
         case 'P':
            tryset(bfd,dupout);
            break;
         case 's':
         case 'S':
            initboard(S,bfd,dupout);
            break;
         case 'q':
         case 'Q':
            endgame(bfd,dupout);
            break;
         case '\n': break;
         default:
            printf("Unknown command\n");
            break;
      }
      if (cmd != '\n')
         while (getchar() != '\n');
   }

   exit(0);
}

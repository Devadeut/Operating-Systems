#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

void printblock ( int B[3][3] )
{
   int i, j;

   printf("\n\n");
   for (i=0; i<3; ++i) {
      printf("  +---+---+---+\n");
      printf("  |");
      for (j=0; j<3; ++j)
         if (B[i][j]) printf(" %d |", B[i][j]);
         else printf("   |");
      printf("\n");
   }
   printf("  +---+---+---+\n");
   fflush(stdout);
}

void readblock ( int A[3][3], int B[3][3] )
{
   int i, j;

   for (i=0; i<3; ++i) {
      for (j=0; j<3; ++j) {
         scanf("%d", &A[i][j]);
         B[i][j] = A[i][j];
      }
   }
   printblock(B);
}

void tryset( int A[3][3], int B[3][3], int r1fd, int r2fd, int c1fd, int c2fd, int dupout, int bfdout ) {
   int i, j, row, col, c, d, errtype = 0;

   scanf("%d%d", &c, &d);
   i = c / 3; j = c % 3;
   if (A[i][j]) errtype = 1;
   if (!errtype) {
      for (row=0; row<3; ++row) {
         for (col=0; col<3; ++col) {
            if ((row == i) && (col == j)) continue;
            if (B[row][col] == d) errtype = 2;
         }
      }
   }
   if (!errtype) {
      close(1); dup(r1fd); printf("R %d %d %d\n", i, d, bfdout);
      scanf("%d", &errtype);
      close(1); dup(dupout);
   }
   if (!errtype) {
      close(1); dup(r2fd); printf("R %d %d %d\n", i, d, bfdout);
      scanf("%d", &errtype);
      close(1); dup(dupout);
   }
   if (!errtype) {
      close(1); dup(c1fd); printf("C %d %d %d\n", j, d, bfdout);
      scanf("%d", &errtype);
      close(1); dup(dupout);
   }
   if (!errtype) {
      close(1); dup(c2fd); printf("C %d %d %d\n", j, d, bfdout);
      scanf("%d", &errtype);
      close(1); dup(dupout);
   }
   if (!errtype) {
      B[i][j] = d;
   } else {
      switch (errtype) {
         case 1: printf("Readonly cell"); break;
         case 2: printf("Block conflict"); break;
         case 3: printf("Row conflict"); break;
         case 4: printf("Column conflict"); break;
         default: printf("Unknown error"); break;
      }
      fflush(stdout);
      sleep(2);
   }
   printblock(B);
}

void rowcheck ( int B[3][3], int dupout )
{
   int i, j, d, fd, errtype = 0;

   scanf("%d%d%d", &i, &d, &fd);
   for (j=0; j<3; ++j) {
      if (B[i][j] == d) errtype = 3;
   }
   close(1); dup(fd);
   printf("%d\n",errtype);
   close(1); dup(dupout);
}

void colcheck ( int B[3][3], int dupout )
{
   int i, j, d, fd, errtype = 0;

   scanf("%d%d%d", &j, &d, &fd);
   for (i=0; i<3; ++i) {
      if (B[i][j] == d) errtype = 4;
   }
   close(1); dup(fd);
   printf("%d\n",errtype);
   close(1); dup(dupout);
}

int main ( int argc, char *argv[] )
{
   int b, bfdin, bfdout, r1fd, r2fd, c1fd, c2fd, dupout;
   char cmd;
   int A[3][3], B[3][3];

   b = atoi(argv[1]);
   bfdin = atoi(argv[2]);
   bfdout = atoi(argv[3]);
   r1fd = atoi(argv[4]);
   r2fd = atoi(argv[5]);
   c1fd = atoi(argv[6]);
   c2fd = atoi(argv[7]);
   dupout = dup(1);
   close(0);
   dup(bfdin);

   printf("Block %d ready\n", b);

   while (1) {
      scanf("%c", &cmd);
      switch (cmd) {
         case 'n':
         case 'N':
            readblock(A,B);
            break;
         case 'p':
         case 'P':
            tryset(A,B,r1fd,r2fd,c1fd,c2fd,dupout,bfdout);
            break;
         case 'r':
         case 'R':
            rowcheck(B,dupout);
            break;
         case 'c':
         case 'C':
            colcheck(B,dupout);
            break;
         case 'q':
         case 'Q':
            printf("Bye...");
            fflush(stdout);
            sleep(2);
            exit(0);
         default:
            printf("\n??? '%c'\n", cmd);
            break;
      }
      while (getchar() != '\n');
   }

   exit(0);
}

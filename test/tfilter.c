#include <stdio.h>
#include <string.h>

extern int filter(char *, char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ tfilter() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   if (argc != 3)
   {
      (void)fprintf(stderr, "Usage: %s <filter> <file-name>\n", argv[0]);
      exit(-1);
   }

   if (filter(argv[1], argv[2]) == 0)
   {
      (void)fprintf(stdout, "GOTHAAA!!!!\n");
   }
   else
   {
      (void)fprintf(stdout, "MISS\n");
   }

   exit(0);
}

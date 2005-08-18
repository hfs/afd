#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "afddefs.h"



/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ arg() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char buffer[MAX_PATH_LENGTH];

   if (get_arg(&argc, argv, "-w", buffer, 1) == INCORRECT)
   {
      (void)fprintf(stderr, "Argument not in list, or some other error!\n");
      exit(INCORRECT);
   }
   else
   {
      register int i;

      (void)fprintf(stdout, "GOTCHA! %s\n\n", buffer);
      (void)fprintf(stdout, "Argument list: <%s", argv[0]);
      for (i = 1; i < argc; i++)
      {
         (void)fprintf(stdout, " %s", argv[i]);
      }
      (void)fprintf(stdout, ">\n");
   }

   exit(SUCCESS);
}

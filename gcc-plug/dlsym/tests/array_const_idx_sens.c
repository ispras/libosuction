#include <stdio.h>
#include <ctype.h>

/* Tests the sensitivy to array index (const values) while traversing function
   body and trying to identify the initial value. The absence of sensitivity gives
   stack overflow */

static int do_meta_command(char *zLine){
  int nArg = 0;
  char *azArg[50];
  while(zLine[1]){
    azArg[nArg++] = &zLine[1];
  }
  if(nArg == 2) azArg[1] = azArg[2];
  const char *zProc = nArg>=3 ? azArg[2] : azArg[1];
  sqlite3_load_extension(0, 0, zProc, 0);
  return 0;
}

int main(int argc, char **argv){
  do_meta_command("smth");
  return 0;
}


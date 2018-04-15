#include "common.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define BASE 10

int parseIntWithError(char *toParse, char *errorMsg) {
  // Try to parse the string as an integer, then print the error if it fails.
  int parsed = strtol(toParse, NULL, BASE);
  if (errno != 0) {
    fputs(errorMsg, stderr);
    exit(1);
  }
  return parsed;
}

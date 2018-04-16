#include "common.h"
#include "sserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Amount of extra space to use for the response buffer. Just in case we get
// more data than we're expecting.
#define FUDGE_AMOUNT 10

int main(int argc, char *argv[]) {
  // Need 6 arguments: The program name, machine name, port, secret key, the
  // variable name, and the value to associate with the variable name.
  if (argc != 6) {
    fprintf(stderr, "Usage: %s <machine name> <port> <secret key> <variable "
                    "name> <value>\n",
            argv[0]);
    exit(1);
  }

  // Parse the arguments and handle any errors that come up.
  char *MachineName = argv[1], *varName = argv[4], *value = argv[5];
  int port;
  int SecretKey;

  port = parseIntWithError(argv[2], "Error: Port must be a number.\n");
  SecretKey =
      parseIntWithError(argv[3], "Error: Secret key must be a number.\n");

  if (strlen(varName) > MAX_VARNAME_LENGTH) {
    fprintf(stderr,
            "Error: Variable name must be at most %d characters (got %zu).\n",
            MAX_VARNAME_LENGTH, strlen(varName));
    exit(1);
  }

  if (strlen(value) > MAX_VALUE_LENGTH) {
    fprintf(stderr, "Error: Value must be at most %u bytes, "
                    "including terminating null (got %zu).\n",
            MAX_VALUE_LENGTH, strlen(value) + 1);
    exit(1);
  }

  int success =
      smallSet(MachineName, port, SecretKey, varName, value, strlen(value) + 1);

  if (success != 0)
    fprintf(stderr, "failed\n");
}

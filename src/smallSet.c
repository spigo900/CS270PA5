#include "common.h"
#include "sserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Amount of extra space to use for the response buffer. Just in case we get
// more data than we're expecting.
#define FUDGE_AMOUNT 10

int main(int argc, char *argv[]) {
  // Need 5 arguments: The program name, machine name, port, secret key, and
  // the value to digest.
  if (argc != 5) {
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
    fprintf(stderr, "Error: Variable name must be at most %d characters.\n",
            MAX_VARNAME_LENGTH);
    exit(1);
  }

  if (strlen(value) > MAX_VALUE_LENGTH) {
    fprintf(stderr, "Error: Value must be at most %u bytes, "
                    "including terminating null (got %zu).\n",
            MAX_VARNAME_LENGTH, strlen(value) + 1);
    exit(1);
  }

  char resultBuf[MAX_RESPONSE_SIZE + FUDGE_AMOUNT];
  int resultLen;
  int success = smallDigest(MachineName, port, SecretKey, value,
                            strlen(value) + 1, resultBuf, &resultLen);

  if (success != 0)
    fprintf(stderr, "failed");
  else
    printf("%s", resultBuf);
}

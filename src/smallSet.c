#include "client_common.h"
#include "sserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum size of the server's response.
#define MAX_RESPONSE_SIZE 100

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

  // Parse the arguments.
  char *MachineName = argv[1], *value = argv[4];
  int port;
  int SecretKey;

  // Read the port, the secret key.
  port = parseIntWithError(argv[2], "Error: Port must be a number.\n");
  SecretKey =
      parseIntWithError(argv[3], "Error: Secret key must be a number.\n");

  char resultBuf[MAX_RESPONSE_SIZE + FUDGE_AMOUNT];
  int resultLen;
  int success = smallDigest(MachineName, port, SecretKey, value,
                            strlen(value) + 1, resultBuf, &resultLen);

  if (success != 0)
    fprintf(stderr, "failed");
  else
    printf("%s", resultBuf);
}

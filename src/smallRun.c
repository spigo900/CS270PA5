#include "common.h"
#include "sserver.h"
#include <stdio.h>
#include <stdlib.h>

// Amount of extra space to use for the response buffer. Just in case we get
// more data than we're expecting.
#define FUDGE_AMOUNT 10

int main(int argc, char *argv[]) {
  // Need 5 arguments: The program name, machine name, port, secret key, and
  // the command to run.
  if (argc != 5) {
    fprintf(stderr, "Usage: %s <machine name> <port> <secret key> <command>\n",
            argv[0]);
    exit(1);
  }

  // Parse the arguments and handle any errors that come up.
  char *MachineName = argv[1], *command = argv[4];
  int port;
  int SecretKey;

  // Read the port, the secret key.
  port = parseIntWithError(argv[2], "Error: Port must be a number.\n");
  SecretKey =
      parseIntWithError(argv[3], "Error: Secret key must be a number.\n");

  char response[MAX_RESPONSE_SIZE + FUDGE_AMOUNT];
  int responseLen;
  int success =
      smallRun(MachineName, port, SecretKey, command, response, &responseLen);
  if (success != 0)
    fprintf(stderr, "failed\n");
  else
    printf("%s\n", response);
}

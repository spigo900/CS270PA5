#include "client_common.h"
#include "sserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum size of the server's response.
#define MAX_RESPONSE_SIZE 100

// Maximum length of a variable name, not including the terminating null.
#define MAX_VARIABLE_NAME_LENGTH 15

// Amount of extra space to use for the response buffer. Just in case we get
// more data than we're expecting.
#define FUDGE_AMOUNT 50

int main(int argc, char *argv[]) {
  // Need 5 arguments: The program name, machine name, port, secret key, and
  // the variable to look up.
  if (argc != 5) {
    fprintf(stderr,
            "Usage: %s <machine name> <port> <secret key> <variable name>\n>",
            argv[0]);
    exit(1);
  }

  // Parse the arguments.
  char *MachineName = argv[1], *varName = argv[4];
  int port;
  int SecretKey;

  // Read the port, the secret key.
  port = parseIntWithError(argv[2], "Error: Port must be a number.\n");
  SecretKey =
      parseIntWithError(argv[3], "Error: Secret key must be a number.\n");
  if (strlen(varName) > MAX_VARIABLE_NAME_LENGTH)
    fprintf(stderr, "Error: Variable name must be at most %d characters.\n",
            MAX_VARIABLE_NAME_LENGTH);

  char resultBuf[MAX_RESPONSE_SIZE + FUDGE_AMOUNT];
  int resultLen;
  // int smallGet(char *MachineName, int port, int SecretKey,
  //        char *variableName, char *value, int *resultLength);
  int success =
      smallGet(MachineName, port, SecretKey, varName, resultBuf, &resultLen);

  if (success != 0)
    fprintf(stderr, "failed");
  else
    printf("%s", resultBuf);
}

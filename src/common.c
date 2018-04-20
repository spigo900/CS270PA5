#include "common.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE 10

int parseIntWithError(char *toParse, const char *errorMsg) {
  // Try to parse the string as an integer, then print the error if it fails.
  int parsed = strtol(toParse, NULL, BASE);
  if (errno != 0) {
    fputs(errorMsg, stderr);
    exit(1);
  }
  return parsed;
}

// VALID_RUN_REQUESTS describes the number of distinct valid run request
// strings. validRunRequests describes the actual valid run request strings.
//
// Using a #define'd constant is not ideal since it could get out of sync with
// the array, but I'm not sure if there's a better solution.
#define VALID_RUN_REQUESTS 3
static const char *validRunRequests[] = {"inet", "hosts", "service"};

// Check whether the given request string is valid (i.e. is one of 'inet',
// 'hosts', or 'services' and is shorter than MAX_RUNREQ_LENGTH).
int isValidRunRequest(char *runRequest) {
  // Check that the string is short enough, first of all.
  if (strlen(runRequest) >= MAX_RUNREQ_LENGTH) return 0;

  // Assume the run request string isn't valid and check if it's actually
  // valid.
  int isValid = 0;

  for (int i = 0; i < VALID_RUN_REQUESTS; i++) {
    int matched = strcmp(runRequest, validRunRequests[i]) == 0;
    isValid |= matched;
    if (isValid)
      break;
  }

  return isValid;
}


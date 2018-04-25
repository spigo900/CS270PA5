#include "sserver.h"
#include "common.h"
#include "csapp.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

// The number of characters to read at a time.
#define REQ_READ_SIZE 200

// For safety, add this amount to the size of any arrays we use.
#define ARRAY_FUDGE_AMOUNT 10

// Send a message to the host described by `machineName` on port `port`, and
// read the response into `response`.
static void sendMessage(char *machineName, int port, void *message,
                        size_t messageLength, void *response,
                        size_t maxResponseSize) {
  int clientfd;
  rio_t rio;

  // Open a connection and set up the Rio type thing. We don't need to check
  // for errors; Rio does it for us.
  clientfd = Open_clientfd(machineName, port);
  Rio_readinitb(&rio, clientfd);

  // Write our message, get the response, then clean up.
  Rio_writen(clientfd, message, messageLength);
  Rio_readnb(&rio, response, maxResponseSize);
  Close(clientfd);
}

// Set the value of variable `variableName` (a null-terminated string) to value
// on the server at MachineName:port, where value is some data of length
// `dataLength`.
int smallSet(char *MachineName, int port, int SecretKey, char *variableName,
             char *value, int dataLength) {
  // Set up some variable names ahead of time.
  int varNameLength = strlen(variableName);

  // If we were given bad input -- the variable name is too long, the data is
  // too long, or we got a negative data length -- signal failure.
  if (varNameLength > MAX_VARNAME_LENGTH || dataLength > MAX_VALUE_LENGTH ||
      dataLength < 0)
    return -1;

  // Set up the message.
  size_t messageLength = CLIENT_PREAMBLE_SIZE + (MAX_VARNAME_LENGTH + 1) +
                         LENGTH_SPECIFIER_SIZE + dataLength;

  ClientSet message;
  message.pre.secretKey = htonl(SecretKey);
  message.pre.msgType = htons(SSERVER_MSG_SET);

  memcpy(&message.varName, variableName, varNameLength + 1);
  message.length = htons(dataLength);
  memcpy(&message.value, value, dataLength);

  // Send our message and get the server's response.
  ServerResponse response;

  sendMessage(MachineName, port, &message, messageLength, &response,
              sizeof(response));

  // Read and return the server's return code.
  int returnCode = (int)response.status;

  return returnCode;
}

// Get the value of variable `variableName` (a null-terminated string) on the
// server at MachineName:port, writing the result to `value` and storing the
// length of the result into the int pointed to by `resultLength`.
int smallGet(char *MachineName, int port, int SecretKey, char *variableName,
             char *value, int *resultLength) {
  // Set up some variable names ahead of time.
  int varNameLength = strlen(variableName);

  // If the given variable name is too long, signal failure.
  if (varNameLength > MAX_VARNAME_LENGTH)
    return -1;

  // Set up the message.
  size_t messageLength = CLIENT_PREAMBLE_SIZE + (MAX_VARNAME_LENGTH + 1);

  ClientGet message;
  message.pre.secretKey = htonl(SecretKey);
  message.pre.msgType = htons(SSERVER_MSG_GET);
  memcpy(&message.varName, variableName, varNameLength + 1);

  // Read the response, copy it, and close the connection.
  ServerResponse response;
  sendMessage(MachineName, port, &message, messageLength, &response,
              sizeof(response));

  // Read and return the server's return code.
  int returnCode = (int)response.status;

  // If we got a failure result, exit early with that result.
  if (returnCode < 0)
    return returnCode;

  // Get the length specifier.
  int valueLen = (int)ntohs(response.length);

  // If specifier is negative or longer than max, signal an error.
  if (valueLen < 0 || valueLen > MAX_VALUE_LENGTH)
    return -1;

  // If the `value` pointer is non-null, copy the result into that buffer. We
  // assume that `value` already points to a valid chunk of memory long enough
  // to hold any value. This might or might not be a good assumption to make...
  // in any case I think we'd have to take a `char**` parameter if we wanted to
  // malloc() the space ourselves.
  if (value != NULL)
    memcpy(value, &response.data, valueLen);

  // If the `resultLength` pointer is non-null, copy the result's length there.
  if (resultLength != NULL)
    *resultLength = valueLen;

  return returnCode;
}

// Get the SHA256 checksum of `data` on the server at MachineName:port and
// write the response to the memory pointed to by `result`, with length written
// to `resultLength`. The result will be at most 100 bytes long.
int smallDigest(char *MachineName, int port, int SecretKey, char *data,
                int dataLength, char *result, int *resultLength) {
  // Return -1 if MAX_DIGEST_LENGTH is longer than expected.
  if (dataLength > MAX_DIGEST_LENGTH)
    return -1;

  // Set up the message and send it.
  size_t messageLength =
      CLIENT_PREAMBLE_SIZE + LENGTH_SPECIFIER_SIZE + dataLength;

  ClientDigest message;
  message.pre.secretKey = htonl(SecretKey);
  message.pre.msgType = htons(SSERVER_MSG_DIGEST);
  message.length = htons(dataLength);
  memcpy(&message.value, data, dataLength);

  // Read the response, copy it, and close the connection.
  ServerResponse response;
  sendMessage(MachineName, port, &message, messageLength, &response,
              sizeof(response));

  // Read and return the server's return code.
  int returnCode = (int)response.status;

  // If we got a failure result, exit early with that result.
  if (returnCode < 0)
    return returnCode;

  // Get the length specifier.
  int resultLen = (int)ntohs(response.length);

  // If specifier is negative or longer than max, signal an error.
  if (resultLen < 0 || resultLen > MAX_SERVER_DATA_LENGTH)
    return -1;

  // If the `result` pointer is non-null, copy the result into that buffer. We
  // assume that `value` already points to a valid chunk of memory long enough
  // to hold any value. This might or might not be a good assumption to make...
  // in any case I think we'd have to take a `char**` parameter if we wanted to
  // malloc() the space ourselves.
  if (result != NULL)
    memcpy(result, &response.data, resultLen);

  // If the `resultLength` pointer is non-null, copy the result's length there.
  if (resultLength != NULL)
    *resultLength = resultLen;

  return returnCode;
}

// Run the program specified by `request` on the server at MachineName:port and
// write the response to the memory pointed to by `result`, with length written
// to `resultLength`. The result will be at most 100 bytes long.
int smallRun(char *MachineName, int port, int SecretKey, char *request,
             char *result, int *resultLength) {
  // If the given request isn't valid, return -1 to signal failure.
  if (!isValidRunRequest(request)) {
    return -1;
  }

  // Set up the message.
  size_t messageLength = CLIENT_PREAMBLE_SIZE + MAX_RUNREQ_LENGTH;

  ClientRun message;
  message.pre.secretKey = htonl(SecretKey);
  message.pre.msgType = htons(SSERVER_MSG_RUN);
  memcpy(&message.request, request, strlen(request) + 1);

  // Send the message and get the response.
  ServerResponse response;
  sendMessage(MachineName, port, &message, messageLength, &response,
              sizeof(response));

  // Read and return the server's return code.
  int returnCode = (int)response.status;

  // If we got a failure result, exit early with that result.
  if (returnCode < 0)
    return returnCode;

  // Get the length specifier.
  int resultLen = (int)ntohs(response.length);

  // If specifier is negative or longer than max, signal an error.
  if (resultLen < 0 || resultLen > MAX_SERVER_DATA_LENGTH)
    return -1;

  // If the `result` pointer is non-null, copy the result into that buffer. We
  // assume that `value` already points to a valid chunk of memory long enough
  // to hold any value. This might or might not be a good assumption to make...
  // in any case I think we'd have to take a `char**` parameter if we wanted to
  // malloc() the space ourselves.
  if (result != NULL)
    memcpy(result, &response.data, resultLen);

  // If the `resultLength` pointer is non-null, copy the result's length there.
  if (resultLength != NULL)
    *resultLength = resultLen;

  return returnCode;
}

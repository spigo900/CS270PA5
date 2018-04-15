#include "sserver.h"
#include "common.h"
#include "csapp.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>

// The number of characters to read at a time.
#define REQ_READ_SIZE 200

// For safety, add this amount to the size of any arrays we use.
#define ARRAY_FUDGE_AMOUNT 10

// Set up the preamble of a message to be sent to the server.
static void setMessagePreamble(char *message, int SecretKey,
                               MessageType msgtype) {
  // Write the integer SecretKey to the first four (0-3) bytes in network (Big
  // Endian) order.
  message[0] = (SecretKey >> 24) & 0xFF;
  message[1] = (SecretKey >> 16) & 0xFF;
  message[2] = (SecretKey >> 8) & 0xFF;
  message[3] = SecretKey & 0xFF;

  // Write out the bytes of the message type. The message type is technically
  // a 2-byte integer following the spec, thus this writes both bytes even
  // though the higher order byte will always be 0. Once again, we write it in
  // network (Big Endian) order.
  message[4] = (msgtype >> 8) & 0xFF;
  message[5] = msgtype & 0xFF;
}

// Set the value of variable `variableName` to value on the server at
// MachineName:port, where value is some data of length `dataLength`.
int smallSet(char *MachineName, int port, int SecretKey, char *variableName,
             char *value, int dataLength) {
  // Set up some variable names ahead of time.
  int varNameLength = strlen(variableName);

  // TODO: Merge all these conditions into one if-statement?
  // If the given variable name is too long, signal failure.
  if (varNameLength > MAX_VARNAME_LENGTH)
    return -1;

  // If the given value is too long, signal failure.
  if (dataLength > MAX_VALUE_LENGTH)
    return -1;

  // If no value was given, signal failure.
  if (dataLength <= 0)
    return -1;

  // Set up the clientfd and Rio ID.
  int clientfd;
  rio_t rio;

  clientfd = Open_clientfd(MachineName, port);
  Rio_readinitb(&rio, clientfd);

  // Set up the message.
  size_t messageLength =
      CLIENT_PREAMBLE_SIZE + varNameLength + LENGTH_SPECIFIER_SIZE + dataLength;
  char *message = malloc(messageLength);

  setMessagePreamble(message, SecretKey, SSERVER_MSG_SET);
  memcpy(&message[CLIENT_PREAMBLE_SIZE], variableName, varNameLength + 1);
  // Write length specifier in network (Big Endian) order.
  message[CLIENT_PREAMBLE_SIZE + varNameLength] = (dataLength >> 8) & 0xFF;
  message[CLIENT_PREAMBLE_SIZE + varNameLength + 1] = dataLength & 0xFF;
  memcpy(&message[CLIENT_PREAMBLE_SIZE + varNameLength + 1], value, dataLength);

  // Don't need to check for errors; Rio does it for us.
  Rio_writen(clientfd, message, messageLength);

  free(message);

  // Read the server's response.
  char resultBuf[MAX_RESPONSE_SIZE];
  Rio_readlineb(&rio, resultBuf, MAX_RESPONSE_SIZE);
  Close(clientfd);

  // Read and return the server's return code.
  int returnCode = (int)resultBuf[0];

  return returnCode;
}

// Get the value of variable `variableName` on the server at MachineName:port,
// writing the result to `value` and storing the length of the result into the
// int pointed to by `resultLength`.
int smallGet(char *MachineName, int port, int SecretKey, char *variableName,
             char *value, int *resultLength) {
  // Set up some variable names ahead of time.
  int varNameLength = strlen(variableName);

  // If the given variable name is too long, signal failure.
  if (varNameLength > MAX_VARNAME_LENGTH)
    return -1;

  // Set up the clientfd and Rio ID.
  int clientfd;
  rio_t rio;

  clientfd = Open_clientfd(MachineName, port);
  Rio_readinitb(&rio, clientfd);

  // Set up the message.
  size_t messageLength = CLIENT_PREAMBLE_SIZE + varNameLength + 1;
  char *message = malloc(messageLength);

  setMessagePreamble(message, SecretKey, SSERVER_MSG_SET);
  memcpy(&message[CLIENT_PREAMBLE_SIZE], variableName, varNameLength + 1);

  // Don't need to check for errors; Rio does it for us.
  Rio_writen(clientfd, message, messageLength);

  // Clean up.
  free(message);

  // Read the response, copy it, and close the connection.
  // TODO: I should probably check up whether Rio_readlineb stops at a null.
  // 'Cause that makes a difference.
  char resultBuf[MAX_RESPONSE_SIZE];
  Rio_readlineb(&rio, resultBuf, MAX_RESPONSE_SIZE);
  Close(clientfd);

  // Read and return the server's return code.
  int returnCode = (int)resultBuf[0];

  // If we got a failure result, exit early with that result.
  if (returnCode < 0)
    return returnCode;

  // Get the length specifier.
  int valueLen = ((int)resultBuf[SERVER_PREAMBLE_SIZE]) << 8;
  valueLen |= (int)resultBuf[SERVER_PREAMBLE_SIZE + 1];

  if (valueLen >= 0 || valueLen < MAX_VALUE_LENGTH)
    return -1;

  // If the `value` pointer is non-null, copy the result into that buffer.
  if (value != NULL)
    memcpy(value, &resultBuf[SERVER_PREAMBLE_SIZE + LENGTH_SPECIFIER_SIZE],
           valueLen);

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

  // Set up the clientfd and Rio ID.
  int clientfd;
  rio_t rio;

  clientfd = Open_clientfd(MachineName, port);
  Rio_readinitb(&rio, clientfd);

  // Set up the message and send it.
  size_t messageLength =
      CLIENT_PREAMBLE_SIZE + LENGTH_SPECIFIER_SIZE + dataLength + 1;
  char *message = malloc(messageLength);

  setMessagePreamble(message, SecretKey, SSERVER_MSG_RUN);
  message[CLIENT_PREAMBLE_SIZE] = (dataLength >> 8) & 0xFF;
  message[CLIENT_PREAMBLE_SIZE + 1] = dataLength & 0xFF;
  memcpy((void *)&message[CLIENT_PREAMBLE_SIZE + LENGTH_SPECIFIER_SIZE],
         (void *)data, strlen(data) + 1);

  // Don't need to check for errors; Rio does it for us.
  Rio_writen(clientfd, message, messageLength);

  free(message);

  // Read the response, copy it, and close the connection.
  char resultBuf[MAX_RESPONSE_SIZE];
  Rio_readlineb(&rio, &resultBuf, MAX_RESPONSE_SIZE);

  // Read and return the server's return code.
  int returnCode = (int)resultBuf[0];
  Close(clientfd);

  // If we got a failure result, exit early with that result.
  if (returnCode < 0)
    return returnCode;

  // Get the length specifier.
  int myResultLength = ((int)resultBuf[SERVER_PREAMBLE_SIZE]) << 8;
  myResultLength |= (int)resultBuf[SERVER_PREAMBLE_SIZE + 1];

  // TODO: should I use MAX_VALUE_LENGTH, or should I have a
  // MAX_RESPONSE_VAL_LENGTH or something?
  //
  // Signal failure if we got a result length that's negative or bigger than
  // the max digest length.
  if (myResultLength < 0 || myResultLength >= MAX_DIGEST_LENGTH)
    return -1;

  // If the `result` pointer is non-null, copy the result into said buffer.
  if (result != NULL) {
    memcpy(result, &resultBuf[SERVER_PREAMBLE_SIZE + LENGTH_SPECIFIER_SIZE],
           myResultLength);
  }

  // If `resultLength` is non-null, fill it with the actual length we got.
  if (resultLength != NULL)
    *resultLength = myResultLength;

  return 0;
}

// Run the program specified by `request` on the server at MachineName:port and
// write the response to the memory pointed to by `result`, with length written
// to `resultLength`. The result will be at most 100 bytes long.
int smallRun(char *MachineName, int port, int SecretKey, char *request,
             char *result, int *resultLength) {
  // If the given request isn't valid, return -1 to signal failure.
  if (!isValidRunRequest(request) ||
      (strlen(request) + 1) > MAX_RUNREQ_LENGTH) {
    return -1;
  }

  // Set up the clientfd and Rio ID.
  int clientfd;
  rio_t rio;

  clientfd = Open_clientfd(MachineName, port);
  Rio_readinitb(&rio, clientfd);

  // Set up the message.
  size_t messageLength = CLIENT_PREAMBLE_SIZE + strlen(request) + 1;
  char *message = malloc(messageLength);

  setMessagePreamble(message, SecretKey, SSERVER_MSG_RUN);
  memcpy((void *)&message[CLIENT_PREAMBLE_SIZE], (void *)request,
         strlen(request) + 1);

  // Don't need to check for errors; Rio does it for us.
  Rio_writen(clientfd, message, messageLength);

  free(message);

  // Read the response, copy it, and close the connection.
  char resultBuf[MAX_RESPONSE_SIZE];
  // NOTE: this doesn't stop at a null as far as I can tell. I'm not sure if
  // that will make a difference.
  Rio_readlineb(&rio, resultBuf, MAX_RESPONSE_SIZE);
  Close(clientfd);

  // Get the length specifier.
  int myResultLength = ((int)resultBuf[SERVER_PREAMBLE_SIZE]) << 8;
  myResultLength |= (int)resultBuf[SERVER_PREAMBLE_SIZE + 1];

  // TODO: should I use MAX_VALUE_LENGTH, or should I have a
  // MAX_RESPONSE_VAL_LENGTH or something?
  if (myResultLength >= 0 || myResultLength < MAX_VALUE_LENGTH)
    return -1;

  // If the `value` pointer is non-null, copy the result into that buffer.
  if (result != NULL)
    memcpy(result, &resultBuf[SERVER_PREAMBLE_SIZE + LENGTH_SPECIFIER_SIZE],
           myResultLength);

  // If the `resultLength` pointer is non-null, copy the result's length there.
  if (resultLength != NULL)
    *resultLength = myResultLength;

  // Copy the result and its length into `result` and `resultLength`,
  // respectively, if they're non-null.
  if (result != NULL) {
    memcpy(result, resultBuf, strlen(resultBuf) + 1);
  }

  if (resultLength != NULL)
    *resultLength = strlen(resultBuf) + 1;

  return 0;
}

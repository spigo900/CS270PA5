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
  Rio_readlineb(&rio, response, maxResponseSize);
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

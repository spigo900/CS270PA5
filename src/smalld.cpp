#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <unistd.h>
extern "C" {
#include "common.h"
#include "csapp.h"
}

using std::map;
using std::string;
using std::cout;
using std::cerr;
using std::endl;

// The size of a connection buffer, used to hold the server's response when
// responding to a client's request.
const size_t CONN_BUFFER_SIZE = 200;

//===================
// Helper functions.
//===================

// Read the preamble of a client's request into a struct.
ClientPreamble readPreamble(char clientRequest[]) {
  // Read the preamble. Have to read byte in network order, i.e. Big Endian.
  ClientPreamble preamble{0, 0};
  preamble.secretKey = ((int)clientRequest[0]) << 24;
  preamble.secretKey |= ((int)clientRequest[1]) << 16;
  preamble.secretKey |= ((int)clientRequest[2]) << 8;
  preamble.secretKey |= ((int)clientRequest[3]);

  preamble.type = clientRequest[4] << 8;
  preamble.type |= clientRequest[5];

  return preamble;
}

// Get the digest value of the value using /bin/sha256sum.
string digest(int valueLength, const char* value) {
  // Set up pipes.
  int stdinCopy = dup(STDIN_FILENO);
  int stdoutCopy = dup(STDOUT_FILENO);

  // Make two pipes: One (`childIn`) we'll use to send `value` to sha256sum;
  // another (`childOut`) that we'll use to read the digest.
  //
  // NOTE: According to man 2 pipe2, [0] is the read end, [1] is write end.
  int childIn[2];
  int childOut[2];
  pipe(childIn);
  pipe(childOut);

  // Overwrite stdin and stdout with pipes.
  dup2(STDIN_FILENO, childIn[0]);
  dup2(STDOUT_FILENO, childOut[1]);
  int status = system("/bin/sha256sum");

  // Restore stdin and stdout, then close the pipes we don't need: the read
  // pipe for childIn, the write pipe for childOut.
  dup2(STDOUT_FILENO, stdoutCopy);
  dup2(STDIN_FILENO, stdinCopy);
  close(childIn[0]);
  close(childOut[1]);

  // Send the value to be digested.
  write(childIn[1], value, valueLength);
  close(childIn[1]);

  // Read the digest and close the read pipe.
  const int FUDGE_AMT = 20;
  char digestBuf[MAX_DIGEST_LENGTH + 1 + FUDGE_AMT];
  digestBuf[MAX_DIGEST_LENGTH] = '\0';

  read(childOut[0], digestBuf, MAX_DIGEST_LENGTH);
  close(childOut[0]);

  // Return the digest result. Remove the trailing newline if there is one.
  string out = digestBuf;
  if (out.size() > 0 && *(out.end() - 1) == '\n')
    out.pop_back();
  return out;
}

// Run a child program, capturing and returning its stdout.
/*
string run(const string &exe, const vector<string> &args) {
  int stdout = dup(STDOUT_FILENO);

  int pipes[2];
  pipe(pipes);
  int writePipe = pipes[0];
  int readPipe = pipes[1];
  dup2(STDIN_FILENO, writePipe);

  string cmd = exe;
  for (const string& arg : args) {
    cmd += " '";
    cmd += arg;
    cmd += "'";
  }

  system(cmd.c_string());
  close(pipes[0])
}
*/

//====================
// Variable storage.
//====================

// TODO: handle. Maybe do this in another file. Copy vars.h, vars.cpp from msh?

//====================
// Response handlers.
//====================

// Our response functions. A response function is intended to take the client's
// file descriptor, the number of bytes in the client's request, the bytes of
// the request (not including the preamble), and a reference to the "detail"
// string), and return whether or not it succeeded in responding to the client.
using ResponseFunction = std::function<bool(int, int, char[], string &)>;
map<MessageType, ResponseFunction> responseFunctions;
void initHandlers();

// Handler for a set response. Should set the variable and respond to the
// client appropriately.
bool setResponse(int clientfd, int requestLen, char clientRequest[],
                 string &detail) {
  char connBuffer[CONN_BUFFER_SIZE];
  rio_t rio;

  // Rea in the variable name, the value length, and the value.
  string varName(&clientRequest[0]);
  int varNameLength = varName.length();
  // TODO: Not sure how to handle this. This is the case where the var name is
  if (varNameLength >= (requestLen - LENGTH_SPECIFIER_SIZE))
    return -1;
  if (varNameLength >= requestLen)
    return -1;

  detail = varName;
  detail += ": ";

  unsigned short valueLength;
  valueLength = ((unsigned short)clientRequest[varNameLength + 1]) << 8;
  valueLength |= (unsigned short)clientRequest[varNameLength + 2];

  // Check the value's length.
  if (valueLength > MAX_VALUE_LENGTH) {
    detail += "(value length field too large, at ";
    detail += valueLength;
    detail += +" bytes)";
    return -1;
  }

  char *value = (char *)malloc(valueLength + 1);
  std::copy(&clientRequest[varNameLength + LENGTH_SPECIFIER_SIZE +
                           1], // +1 for the trailing null
            &clientRequest[varNameLength + LENGTH_SPECIFIER_SIZE +
                           valueLength], // do I need a +1 here?
            value);

  detail += value;

  // TODO: Set var, return whether it exists, else return -1.

  // TODO: Handle response. Somehow.

  return 0;
}

// Handler for a get response. Should get the variable and return it to the
// client appropriately.
bool getResponse(int clientfd, int requestLen, char clientRequest[],
                 string &detail) {
  char connBuffer[CONN_BUFFER_SIZE];
  rio_t rio;

  string varName(&clientRequest[0]);
  int varNameLength = varName.length();
  detail = varName;
  if (varNameLength >= requestLen)
    return -1;

  // TODO: Lookup var, return it if it exists, else return -1.

  return 0;
}

// Digest response handler. Should process the input appropriately and return
// the digest to the client.
bool digestResponse(int clientfd, int requestLen, char clientRequest[],
                    string &detail) {
  int result = 0;

  // Read in the value length in network order.
  unsigned short valueLength;
  valueLength = ((unsigned short)clientRequest[0]) << 8;
  valueLength |= (unsigned short)clientRequest[1];

  // If the value's length is out of range, 
  if (valueLength > MAX_VARNAME_LENGTH) {
    result = -1;
  }

  // Make sure we actually got the expected number of bytes.
  if (valueLength >= (requestLen - LENGTH_SPECIFIER_SIZE))
    result = -1;

  // Grab the value and copy it into the detail string.
  char *value = &clientRequest[LENGTH_SPECIFIER_SIZE];
  detail = value;

  // Maximum length of a command.
  /*
  const int MAX_COMMAND_LENGTH = 60 + MAX_VALUE_LENGTH;
  // NOTE: Is snprintf a security vulnerability here?
  char command[MAX_COMMAND_LENGTH];
  snprintf(command, MAX_COMMAND_LENGTH, "/bin/echo '%100s' | /bin/sha256sum", value);

  // Copy the value.
  //
  // TODO: I might not need this.
  char *value = (char *)malloc(valueLength + 1);
  std::copy(&clientRequest[LENGTH_SPECIFIER_SIZE],
            &clientRequest[LENGTH_SPECIFIER_SIZE + valueLength - 1],
            value);
  */


  // Get the digest.
  string out = digest(valueLength, value);

  char connBuffer[CONN_BUFFER_SIZE];
  connBuffer[0] = (char) result;

  // The length of out, including the final null.
  int outLengthNull = out.length() + 1;
  std::copy(
      out.begin(),
      out.begin() + (outLengthNull > MAX_DIGEST_LENGTH ? MAX_DIGEST_LENGTH : outLengthNull),
      &connBuffer[SERVER_PREAMBLE_SIZE + LENGTH_SPECIFIER_SIZE]);
  connBuffer[SERVER_PREAMBLE_SIZE + LENGTH_SPECIFIER_SIZE + outLengthNull] = '\0';

  // Write out the response.
  Rio_writen(clientfd, connBuffer, outLengthNull);

  return 0;
}

// Handler for a run response. Should check that the request is valid, and if
// so, run the appropriate program and return the result to the client.
bool runResponse(int clientfd, int requestLen, char clientRequest[],
                 string &detail) {
  char connBuffer[CONN_BUFFER_SIZE];
  rio_t rio;

  string varName(&clientRequest[0]);
  int varNameLength = varName.length();
  detail = varName;
  if (varNameLength >= requestLen)
    return -1;

  // TODO: Handle.

  return 0;
}

// Setup the handlers table.
void initHandlers() {
  responseFunctions[SSERVER_MSG_SET] = setResponse;
  responseFunctions[SSERVER_MSG_GET] = getResponse;
  responseFunctions[SSERVER_MSG_DIGEST] = digestResponse;
  responseFunctions[SSERVER_MSG_RUN] = runResponse;
};

// Lookup a handler in the handlers table.
ResponseFunction lookupHandler(MessageType type) {
  auto it = responseFunctions.find(type);
  ResponseFunction handler = [=](int _clientfd, int _len, char _request[],
                                 string &detail) -> bool {
    detail = "error";
    cerr << "Error: No appropriate handler for message of type `" << type
         << "`." << endl;

    return false;
  };
  if (it != responseFunctions.end())
    handler = it->second;
  return handler;
}

//=====================
// Request type names.
//=====================

map<MessageType, string> requestTypeNames;

// Set up the request type names.
void initRequestTypeNames() {
  requestTypeNames[SSERVER_MSG_SET] = "set";
  requestTypeNames[SSERVER_MSG_GET] = "get";
  requestTypeNames[SSERVER_MSG_DIGEST] = "digest";
  requestTypeNames[SSERVER_MSG_RUN] = "run";
}

// Get the name of a request type.
const string &getRequestTypeName(MessageType request) {
  static const string empty;
  auto it = requestTypeNames.find(request);
  string out;
  if (it != requestTypeNames.end())
    out = it->second;
  return (out.size() == 0) ? empty : it->second;
}

//==============================
// Actual server functionality.
//==============================

void handleClient(int connfd, unsigned int secretKey) {
  // Read in the client's request and parse it.
  char clientRequest[MAX_REQUEST_SIZE];
  rio_t rio;
  Rio_readinitb(&rio, connfd);
  int requestLen = (int)Rio_readlineb(&rio, clientRequest, MAX_REQUEST_SIZE);

  // TODO: Handle the case where we got a too-short request.
  if (requestLen < CLIENT_PREAMBLE_SIZE) /* HANDLE */
    ;

  // Read the preamble
  ClientPreamble preamble = readPreamble(clientRequest);

  // TODO: Do something if the client's secret key isn't right.
  if (preamble.secretKey != secretKey) {
    // STUFF
  };

  // Handle the actual request.
  ResponseFunction handler = lookupHandler((MessageType)preamble.type);

  string detail;
  bool status = handler(connfd, requestLen - CLIENT_PREAMBLE_SIZE, clientRequest, detail);
  string statusGloss = status ? "success" : "failure";

  // Log request information. Could possibly be extracted into another function
  // to make this one shorter, but it's not used anywhere else, so I'm not sure
  // if it's worth it.
  cerr << "Secret key = " << preamble.secretKey << endl
       << "Request type = " << getRequestTypeName((MessageType)preamble.type) << endl
       << "Detail = " << detail << endl
       << "Completion = " << statusGloss << endl
       << "--------------------------" << endl;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    cerr << "Usage: " << argv[0] << " [port] [secret key]" << endl;
    exit(1);
  }

  // Parse the arguments.
  int port;
  unsigned int secretKey;

  // NOTE: Exits if we encounter an error!
  port = parseIntWithError(argv[1], "Error: Port must be a number.\n");
  secretKey =
      parseIntWithError(argv[2], "Error: Secret key must be a number.\n");

  initRequestTypeNames();

  // BEGIN SHAMELESSLY COPIED CODE
  int listenfd, connfd;
  // socklen_t clientlen;
  // struct sockaddr_in clientaddr;

  listenfd = Open_listenfd(port);
  while (true) {
    // clientlen = sizeof(clientaddr);
    // connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    connfd = Accept(listenfd, NULL, NULL);

    /* Determine the domain name and IP address of the client */
    handleClient(connfd, secretKey);

    Close(connfd);
  }

  // END SHAMELESSLY COPIED CODE
  return 0;
}

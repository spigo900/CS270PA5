#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>
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
  // Read the preamble from the bytes. Don't bother swapping them to host
  // order; we'll do that later.
  ClientPreamble preamble{0, 0};
  // because sizeof int == 4, this reads exactly what we want
  preamble.secretKey = *(unsigned int *)&clientRequest[0];

  preamble.msgType =
      *(unsigned short *)&clientRequest[4];
  return preamble;
}

// Get the digest value of the value using /bin/sha256sum.
string digest(int valueLength, const char *value) {
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

  if (status != 0)
	printf("WARNING: sha2457sum did not exit with status 0.");

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

  // Return the digest result. Remove thetrailing newline if there is one.
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

map<string, string> storedVars;

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
	cout << "CALLING SET RESPONSE" << endl;
  char connBuffer[CONN_BUFFER_SIZE];
  rio_t rio;
  Rio_readinitb(&rio, clientfd);

  char name[15];
  cout << "trying to get name: " << endl;
  int nameLen = (int)Rio_readlineb(&rio, clientRequest, 15);
  cout << "deteccted name: " << name << endl;

  // Read in the variable name, the value length, and the value.
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

  storedVars[varName] = value;

  cout << "stored " << value <<  " as " << varName << endl;

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
  snprintf(command, MAX_COMMAND_LENGTH, "/bin/echo '%100s' | /bin/sha256sum",
  value);

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
  connBuffer[0] = (char)result;

  // The length of out, including the final null.
  int outLengthNull = out.length() + 1;
  std::copy(out.begin(),
            out.begin() + (outLengthNull > MAX_DIGEST_LENGTH ? MAX_DIGEST_LENGTH
                                                             : outLengthNull),
            &connBuffer[SERVER_PREAMBLE_SIZE + LENGTH_SPECIFIER_SIZE]);
  connBuffer[SERVER_PREAMBLE_SIZE + LENGTH_SPECIFIER_SIZE + outLengthNull] =
      '\0';

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
  cout << SSERVER_MSG_SET << "sserver get" << endl;
};

// Lookup a handler in the handlers table.
ResponseFunction lookupHandler(MessageType type) {
  auto it = responseFunctions.find(type);

  if (it != responseFunctions.end())
    return it->second;

  return [=](int _clientfd, int _len, char _request[],
                                 string &detail) -> bool {
    detail = "error";
    cerr << "Error: No appropriate handler for message of type `" << type
         << "`." << endl;

    return false;
  };
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
  cout << "reading..." << endl;
  // only read preamble -- rest will be handled later
  // as it has variable length
  int requestLen = (int)Rio_readnb(&rio, clientRequest, 8);
  cout << "done reading (" << requestLen << ")" << endl;

  // TODO: Handle the case where we got a too-short request.
  if (requestLen < CLIENT_PREAMBLE_SIZE)
    cout << "WARNING: request too short: " << requestLen << endl;

  // Read the preamble and process the info to get the host-order information
  // we need.
  ClientPreamble preamble = readPreamble(clientRequest);
  unsigned int theirKey = ntohl(preamble.secretKey);
  MessageType rqType = (MessageType)ntohs(preamble.msgType);

  // TODO: Do something if the client's secret key isn't right.
  if (theirKey != secretKey) {
	  cout << "Incorrect Key; Access denied." << endl;
	  exit(1);
  };

  // Handle the actual request.
  ResponseFunction handler = lookupHandler(rqType);

  string detail;
  cout << "CALLING AHANDLER" << endl;
  bool status = handler(connfd, requestLen - CLIENT_PREAMBLE_SIZE,
                        &clientRequest[CLIENT_PREAMBLE_SIZE], detail);
  string statusGloss = status ? "success" : "failure";

  // Log request information. Could possibly be extracted into another function
  // to make this one shorter, but it's not used anywhere else, so I'm not sure
  // if it's worth it.
  cerr << "Secret key = " << ntohl(preamble.secretKey) << endl
       << "Request type = " << getRequestTypeName(rqType) << endl
       << "Detail = " << detail << endl
       << "Completion = " << statusGloss << endl
       << "--------------------------" << endl;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    cerr << "Usage: " << argv[0] << " [port] [secret key]" << endl;
    exit(1);
  }

  //initialize map of lambdas
  initHandlers();

  // Parse the arguments.
  int port;
  unsigned int secretKey;

  // NOTE: Exits if we encounter an error!
  port = parseIntWithError(argv[1], "Error: Port must be a number.\n");
  secretKey =
      parseIntWithError(argv[2], "Error: Secret key must be a number.\n");

  initRequestTypeNames();

  // BEGIN SHAMELESSLY COPIED CODE
  int listenfd, connfd, listenPort;
  sockaddr_in clientAddr;
  hostent *clientHostEntry;
  char *clientIP;
  unsigned short clientPort;
  listenfd = Open_listenfd(port);
  socklen_t addrLength = sizeof(clientAddr);

  while (true) {
    // clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientAddr, &addrLength);
    cout << "created connfd: " << connfd << endl;

    /* Determine the domain name and IP address of the client */
    handleClient(connfd, secretKey);

    Close(connfd);
  }

  // END SHAMELESSLY COPIED CODE
  return 0;
}

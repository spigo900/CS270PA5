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
const size_t CONN_BUFFER_SIZE = 400;

using ValueT = std::array<char, MAX_VALUE_LENGTH>;

//===================
// Helper functions.
//===================

// Helper that lets us print a value using the << operator on streams.
std::ostream& operator<<(std::ostream& out, ValueT val) {
  for (char valChar : val) {
    out << valChar;
  }
  return out;
}

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

map<string, ValueT> storedVars;

//====================
// Response handlers.
//====================

// Our response functions. A response function is intended to take the client's
// file descriptor, the number of bytes in the client's request, the bytes of
// the request (not including the preamble), and a reference to the "detail"
// string), and return whether or not it succeeded in responding to the client.
using ResponseFunction = std::function<bool(int, rio_t, string &)>;
map<MessageType, ResponseFunction> responseFunctions;
void initHandlers();

void fail(int clientfd) {
  ServerResponse response;
  response.status = -1;
  Rio_writen(clientfd, &response, sizeof(response));
}

// Handler for a set response. Should set the variable and respond to the
// client appropriately.
bool setResponse(int clientfd, rio_t rio, string &detail) {
  char name[MAX_VARNAME_LENGTH + 1];
  Rio_readnb(&rio, &name[0], MAX_VARNAME_LENGTH + 1);

  // Read in the variable name and calculate its length.
  string varName(name);
  int varNameLength = varName.length();

  // Set the detail string.
  detail = varName;

  // Check the value's length.
  if (varNameLength > MAX_VARNAME_LENGTH) {
    fail(clientfd);
    return false;
  }

  // Add a colon before the value.
  detail += ": ";

  unsigned short valueLength;
  Rio_readnb(&rio, &valueLength, sizeof(valueLength));
  valueLength = ntohs(valueLength);

  // Check the value's length.
  if (valueLength > MAX_VALUE_LENGTH) {
    detail += "(value length field too large, at ";
    detail += valueLength;
    detail += " bytes)";
    fail(clientfd);
    return false;
  }

  // Read the value, and add it to the detail string.
  ValueT value;
  Rio_readnb(&rio, &value[0], valueLength);

  for (char valChar : value) { detail += valChar; }

  // Store the value and print a debug message about it.
  storedVars[varName] = value;

  cerr << "stored " << value <<  " as " << varName << endl;

  ServerResponse response;
  response.status = 0;
  Rio_writen(clientfd, &response, sizeof(response));

  return true;
}

// Handler for a get response. Should get the variable and return it to the
// client appropriately.
bool getResponse(int clientfd, rio_t rio, string &detail) {
  char connBuffer[CONN_BUFFER_SIZE];
  // rio_t rio;

  // Read the var name requested in as varName. Set the detail string
  // appropriately.
  string varName;
  int varNameLength = varName.length();
  detail = varName;

  // If it's not a valid variable name, reject.
  if (varNameLength > MAX_VARNAME_LENGTH) {
    fail(clientfd);
    return false;
  };

  // Lookup var. Fail if it doesn't exist.
  auto it = storedVars.find(varName);
  if (it == storedVars.end()) {
    fail(clientfd);
    return false;
  }

  // Get the value and respond.
  ValueT value = it->second;

  ServerResponse response;
  response.status = 0;
  response.length = htons((unsigned short) value.size());
  std::copy(value.begin(), value.end(), &response.data[0]);
  Rio_writen(clientfd, &response, sizeof(response));

  return true;
}

// Digest response handler. Should process the input appropriately and return
// the digest to the client.
bool digestResponse(int clientfd, rio_t rio, string &detail) {
  // TODO: write.

  return false;
}

// Handler for a run response. Should check that the request is valid, and if
// so, run the appropriate program and return the result to the client.
bool runResponse(int clientfd, rio_t rio, string &detail) {
  char connBuffer[CONN_BUFFER_SIZE];
  // rio_t rio;

  string varName;
  varName.reserve(MAX_VARNAME_LENGTH + 1);
  int varNameLength = varName.length();
  detail = varName;

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

  return [=](int _clientfd, rio_t _rio, string &detail) -> bool {
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

  // Only read preamble. Since the actual request may vary in size, read that
  // in the handler functions.
  int requestLen = (int)Rio_readnb(&rio, clientRequest, CLIENT_PREAMBLE_SIZE);
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
  bool status = handler(connfd, rio, detail);
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

  // Initialize request handler map.
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
  int listenfd, connfd;
  listenfd = Open_listenfd(port);

  while (true) {
    // connfd = Accept(listenfd, (SA *)&clientAddr, &addrLength);
    // We don't care about the client's address, so don't get it. Just get a
    // connection file descriptor.
    connfd = Accept(listenfd, NULL, NULL);
    cout << "created connfd: " << connfd << endl;

    /* Determine the domain name and IP address of the client */
    handleClient(connfd, secretKey);

    Close(connfd);
  }

  // END SHAMELESSLY COPIED CODE
  return 0;
}

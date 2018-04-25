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
  const char cmdTemplate[] = "echo '%100s' | /usr/bin/sha256sum";
  const size_t CMD_BUFFER_SIZE = 400;
  char cmd[CMD_BUFFER_SIZE];
  snprintf(cmd, CMD_BUFFER_SIZE, cmdTemplate, value);

  FILE* pipe = popen(cmd, "r");
  char digested[MAX_SERVER_DATA_LENGTH];
  fread(&digested[0], sizeof(char), MAX_SERVER_DATA_LENGTH, pipe);
  pclose(pipe);

  // Return the digest result. Remove the trailing newline if there is one.
  string out = digested;
  if (out.size() > 0 && *(out.end() - 1) == '\n')
    out.pop_back();
  return out;
}

// Run a child program, capturing and returning its stdout.
string run(const string &exe) {
  cerr << "CMD IS " << exe << endl;
  string out;
  string cmd = exe;

  // Execute the command and read its output, then return it.
  FILE* cmdOutput = popen(cmd.c_str(), "r");

  char buffer[MAX_SERVER_DATA_LENGTH];
  fread(&buffer[0], sizeof(char), MAX_SERVER_DATA_LENGTH, cmdOutput);
  int st = pclose(cmdOutput);
  cerr << "STATUS WAS " << st << endl;
  out = buffer;
  cerr << "CHECK MY OUT: " << out << endl;

  return out;
}

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
  char name[MAX_VARNAME_LENGTH + 1];
  Rio_readnb(&rio, &name[0], MAX_VARNAME_LENGTH + 1);

  // Read in the variable name.
  string varName(name);
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
  // Read in the value length.
  unsigned short valueLength;
  Rio_readnb(&rio, &valueLength, sizeof(valueLength));
  valueLength = ntohs(valueLength);

  // Verify that we got fewer than max characters; fail if not.
  if (valueLength > MAX_DIGEST_LENGTH) {
    fail(clientfd);
    return false;
  }

  // Read in the actual value.
  char value[MAX_DIGEST_LENGTH];
  Rio_readnb(&rio, &value[0], valueLength);

  // Digest it.
  string digested = digest((int) valueLength, (const char*) &value[0]);

  // Respond.
  ServerResponse response;
  response.status = 0;
  response.length = htons((unsigned short) digested.size());
  std::copy(digested.begin(), digested.end(), &response.data[0]);
  Rio_writen(clientfd, &response, sizeof(response));

  return true;
}

// Handler for a run response. Should check that the request is valid, and if
// so, run the appropriate program and return the result to the client.
bool runResponse(int clientfd, rio_t rio, string &detail) {
  char name[MAX_RUNREQ_LENGTH];
  Rio_readnb(&rio, &name[0], MAX_RUNREQ_LENGTH);

  string realName(name);

  string output;
  if (realName == "inet") {
    output = run("/sbin/ifconfig -a");
  } else if (realName == "hosts") {
    output = run("/bin/cat /etc/hosts");
  } else if (realName == "service") {
    output = run("/bin/cat /etc/services");
  } else {
    fail(clientfd);
    return false;
  }

  ServerResponse response;
  response.status = 0;
  response.length = htons((unsigned short) output.size());
  if (output.size() > MAX_SERVER_DATA_LENGTH) {
    std::copy(output.begin(), output.begin() + MAX_SERVER_DATA_LENGTH, &response.data[0]);
    response.data[MAX_SERVER_DATA_LENGTH] = '\0';
  } else {
    std::copy(output.begin(), output.end(), &response.data[0]);
  }
  Rio_writen(clientfd, &response, sizeof(response));

  return true;
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

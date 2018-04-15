#include <iostream>
#include <map>
#include <vector>
extern "C" {
#include "csapp.h"
}

using std::cout;
using std::cerr;
using std::endl;


int main(int argc, char* argv[]) {
  if (argc < 2) {
    cerr << "Usage: smalld [port] [secret key]" << endl;
    exit(1);
  }
}

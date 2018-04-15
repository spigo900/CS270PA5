#include "vars.h"

#include <functional>
#include <iostream>
#include <map>
#include <string>

// use unordered map maybe?
using std::function;
using std::map;
using std::string;

// map of names to values
static map<string, string> vmap;

// if not found, return cb()
string vars::lookup(string name, function<string(void)> cb) {
  auto i = vmap.find(name);
  if (i == vmap.end())
    return cb();
  return i->second;
}

void vars::deleteByName(string name, function<void(void)> cb) {
  auto i = vmap.find(name);
  if (i == vmap.end())
    cb();
  vmap.erase(i);
}

void vars::setVar(string name, string value) { vmap[name] = value; }

void vars::forEach(function<void(string, string)> fn) {
  for (auto i : vmap)
    fn(std::get<0>(i), std::get<1>(i));
}

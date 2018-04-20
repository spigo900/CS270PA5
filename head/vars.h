/*
 * Vars.h - functions for managing (changing + addition + deletion) stored
 * variables in the server
 *
 * author: Owen McGrath
 */
#pragma once
#include <functional>
#include <string>

using std::function;
using std::string;

namespace vars {
/*
 * call notFound and return result if name is not declared
 * "Not declared" is different from "is """
 */
string lookup(string name,
              function<string(void)> notFound = []() { return ""; });

/*
 * This is very rarely called
 */
void deleteByName(string name, function<void(void)> notFoundCallback);

void setVar(string name, string value);

/*
 * I don't think this is ever used
 */
void forEach(function<void(string, string)>);

} // namespace vars

#ifndef FSUTILS_H
#define FSUTILS_H

#include <string>
#include <vector>
#include <iostream>
#include <fstream>

bool file_exists(const std::string &fn);
bool read_entire_file(const std::string &filename, std::string &contents);

#endif

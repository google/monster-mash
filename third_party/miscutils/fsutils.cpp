#include "fsutils.h"
#include <fstream>
#include <sstream>

using namespace std;

bool file_exists(const string &fn)
{
  ifstream myfile(fn);
  if (!myfile.is_open()) {
    return false;
  }
  myfile.close();
  return true;
}

bool read_entire_file(const std::string &filename, std::string &contents)
{
  ifstream stream;
  stream.open(filename);
  if (!stream) return false;
  stringstream buffer;
  buffer << stream.rdbuf();
  contents = buffer.str();
  return true;
}

#pragma once

#include <stdio.h>
#include <string>

std::string readFile(const std::string& file)
{
  std::string ret;
  auto f = fopen(file.c_str(), "rb");
  if (!f)
    return ret;
  fseek(f, 0, SEEK_END);
  uint64_t len = ftell(f);
  fseek(f, 0, SEEK_SET);
  ret.resize(len, 0);
  fread(&ret[0], 1, len, f);
  fclose(f);
  return ret;
}

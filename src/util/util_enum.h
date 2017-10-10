#pragma once

#define ENUM_NAME(name) \
  case name: os << #name; break

#define ENUM_DEFAULT(name) \
  default: os << e

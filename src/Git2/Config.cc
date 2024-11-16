#include "Config.hpp"

#include "Exception.hpp"

#include <string>

namespace git2 {

Config::Config() {
  git2Throw(git_config_new(&mRaw));
}

Config::Config(git_config* raw) : mRaw(raw) {}

Config::~Config() {
  git_config_free(mRaw);
}

Config&
Config::openDefault() {
  git2Throw(git_config_open_default(&mRaw));
  return *this;
}

std::string
Config::getString(const std::string& name) {
  git_buf ret = { nullptr, 0, 0 };
  git2Throw(git_config_get_string_buf(&ret, mRaw, name.c_str()));
  return { ret.ptr, ret.size };
}

} // end namespace git2

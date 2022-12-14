// internal
#include "poac/util/semver/token.hpp"

namespace semver {

auto to_string(const Identifier& id) -> std::string {
  if (std::holds_alternative<Identifier::NumericType>(id.component)) {
    return std::to_string(id.get_numeric());
  } else if (std::holds_alternative<Identifier::AlphanumericType>(id.component
             )) {
    return std::string(id.get_alpha_numeric());
  }
  __builtin_unreachable();
}

auto Version::get_version() const -> std::string {
  std::string version = std::to_string(major);
  version += "." + std::to_string(minor);
  version += "." + std::to_string(patch);
  if (!pre.empty()) {
    version += "-";
    for (const Identifier& s : pre) {
      version += to_string(s) + ".";
    }
    version.pop_back();
  }
  return version;
}

auto Version::get_full() const -> std::string {
  std::string full = get_version();
  if (!build.empty()) {
    full += "+";
    for (const Identifier& s : build) {
      full += to_string(s) + ".";
    }
    full.pop_back();
  }
  return full;
}

} // end namespace semver

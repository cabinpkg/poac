#pragma once

#include <cstddef>
#include <filesystem>
#include <fmt/format.h>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace fs = std::filesystem;

// NOLINTBEGIN(google-global-names-in-headers)
using std::literals::string_literals::operator""s;
using std::literals::string_view_literals::operator""sv;
// NOLINTEND(google-global-names-in-headers)

inline fs::path
operator""_path(const char* str, size_t /*unused*/) {
  return str;
}

[[noreturn]] inline void
panic(
    const std::string_view msg,
    const std::source_location& loc = std::source_location::current()
) {
  throw std::logic_error(
      fmt::format("panicked at '{}', {}:{}\n", msg, loc.file_name(), loc.line())
  );
}

[[noreturn]] inline void
unreachable(
    [[maybe_unused]] const std::source_location& loc =
        std::source_location::current()
) noexcept {
#ifdef NDEBUG
  __builtin_unreachable();
#else
  panic("unreachable", loc);
#endif
}

#ifndef POAC_CMD_LOGIN_HPP
#define POAC_CMD_LOGIN_HPP

// std
#include <fstream>

// external
#include <spdlog/spdlog.h>
#include <structopt/app.hpp>

// internal
#include <poac/config.hpp>
#include <poac/poac.hpp>
#include <poac/util/net.hpp>
#include <poac/util/termcolor2/termcolor2.hpp>

namespace poac::cmd::login {

struct Options : structopt::sub_command {
  /// API Token obtained on poac.pm
  String api_token;
};

using InvalidAPIToken = Error<"invalid API token provided">;
using FailedToLogIn = Error<"failed to log in; API token might be incorrect">;

[[nodiscard]] Result<void>
check_token(StringRef api_token) {
  spdlog::trace("Checking if api_token has 32 length");
  if (api_token.size() != 32) {
    return Err<InvalidAPIToken>();
  }
  spdlog::trace("Checking if api_token exists");
  if (!util::net::api::login(api_token).unwrap_or(false)) {
    return Err<FailedToLogIn>();
  }
  return Ok();
}

[[nodiscard]] Result<void>
exec(const Options& opts) {
  Try(check_token(opts.api_token));

  // Write API Token to `~/.poac/credentials` as TOML
  spdlog::trace("Exporting the api_token to `{}`", config::path::cred_file);
  std::ofstream ofs(config::path::cred_file, std::ofstream::trunc);
  ofs << format("[registry]\ntoken = \"{}\"\n", opts.api_token);

  spdlog::info("{:>25} token for `{}` saved", "Login"_bold_green, "poac.pm");
  return Ok();
}

} // namespace poac::cmd::login

STRUCTOPT(poac::cmd::login::Options, api_token);

#endif // !POAC_CMD_LOGIN_HPP
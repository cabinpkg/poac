#ifndef POAC_CMD_BUILD_HPP
#define POAC_CMD_BUILD_HPP

// std
#include <filesystem>
#include <string>
#include <optional>

// external
#include <mitama/result/result.hpp>
#include <mitama/anyhow/anyhow.hpp>
#include <mitama/thiserror/thiserror.hpp>
#include <toml.hpp>
#include <structopt/app.hpp>

// internal
#include <poac/core/builder.hpp>
#include <poac/core/resolver.hpp>
#include <poac/core/validator.hpp>

namespace poac::cmd::build {
    namespace anyhow = mitama::anyhow;
    namespace thiserror = mitama::thiserror;

    struct Options: structopt::sub_command {
        /// Build artifacts in release mode, with optimizations
        std::optional<bool> release = false;
    };

    [[nodiscard]] anyhow::result<std::filesystem::path>
    build(const Options& opts, const toml::value& config) {
        const auto resolved_deps = MITAMA_TRY(
            core::resolver::install_deps(config)
                .map_err([](const std::string& e){ return anyhow::anyhow(e); })
        );

        using core::builder::mode_t;
        const mode_t mode = opts.release.value() ? mode_t::release : mode_t::debug;
        const std::filesystem::path output_path = MITAMA_TRY(
            core::builder::build(config, mode, resolved_deps)
                .map_err([](const std::string& e){ return anyhow::anyhow(e); })
        );
        return mitama::success(output_path);
    }

    [[nodiscard]] anyhow::result<void>
    exec(const Options& opts) {
        MITAMA_TRY(
            core::validator::required_config_exists()
                .map_err([](const std::string& e){ return anyhow::anyhow(e); })
        );
        const toml::value config = toml::parse("poac.toml");
        MITAMA_TRY(
            build(opts, config).with_context([]{
                return anyhow::anyhow("Failed to build");
            })
        );
        return mitama::success();
    }
} // end namespace

STRUCTOPT(poac::cmd::build::Options, release);

#endif // !POAC_CMD_BUILD_HPP

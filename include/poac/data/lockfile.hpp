#ifndef POAC_DATA_LOCKFILE_HPP
#define POAC_DATA_LOCKFILE_HPP

// std
#include <fstream>
#include <utility>

// external
#include <toml.hpp>

// internal
#include <poac/poac.hpp>
#include <poac/core/resolver/resolve.hpp>
#include <poac/data/manifest.hpp>

namespace poac::data::lockfile {
    inline const String lockfile_name = "poac.lock";
    inline const String lockfile_header =
        " This file is automatically generated by Poac.\n"
        "# It is not intended for manual editing.";

    class Error {
        template <thiserror::fixed_string S, class ...T>
        using error = thiserror::error<S, T...>;

    public:
        using InvalidLockfileVersion =
            error<"invalid lockfile version found: {0}", i64>;

        using FailedToReadLockfile =
            error<"failed to read lockfile:\n{0}", String>;
    };

    inline fs::file_time_type
    poac_lock_last_modified(const fs::path& base_dir) {
        return fs::last_write_time(base_dir / lockfile_name);
    }

    inline bool
    is_outdated(const fs::path& base_dir) {
        if (!fs::exists(base_dir / lockfile_name)) {
            return true;
        }
        return poac_lock_last_modified(base_dir)
             < manifest::poac_toml_last_modified(base_dir);
    }
} // end namespace

namespace poac::data::lockfile::inline v1 {
    namespace resolver = core::resolver::resolve;

    inline constexpr i64 lockfile_version = 1;

    struct Package {
        String name;
        String version;
        Vec<String> dependencies;
    };

    struct Lockfile {
        i64 version = lockfile_version;
        Vec<Package> package;
    };
} // end namespace

TOML11_DEFINE_CONVERSION_NON_INTRUSIVE(
    poac::data::lockfile::v1::Package, name, version, dependencies
)
TOML11_DEFINE_CONVERSION_NON_INTRUSIVE(
    poac::data::lockfile::v1::Lockfile, version, package
)

namespace poac::data::lockfile::inline v1 {
    // -------------------- INTO LOCKFILE --------------------

    [[nodiscard]] Result<toml::basic_value<toml::preserve_comments>>
    convert_to_lock(const resolver::UniqDeps<resolver::WithDeps>& deps) {
        Vec<Package> packages;
        for (const auto& [pack, inner_deps] : deps) {
            Package p{
                resolver::get_name(pack),
                resolver::get_version(pack),
                Vec<String>{},
            };
            if (inner_deps.has_value()) {
                // Extract name from inner dependencies and drop version.
                Vec<String> ideps;
                for (const auto& [name, _v] : inner_deps.value()) {
                    static_cast<void>(_v);
                    ideps.emplace_back(name);
                }
                p.dependencies = ideps;
            }
            packages.emplace_back(p);
        }

        toml::basic_value<toml::preserve_comments> lock(
            Lockfile{ .package = packages },
            { lockfile_header }
        );
        return Ok(lock);
    }

    [[nodiscard]] Result<void>
    overwrite(const resolver::UniqDeps<resolver::WithDeps>& deps) {
        const auto lock = tryi(convert_to_lock(deps));
        std::ofstream lockfile(config::path::current / lockfile_name, std::ios::out);
        lockfile << lock;
        return Ok();
    }

    [[nodiscard]] Result<void>
    generate(const resolver::UniqDeps<resolver::WithDeps>& deps) {
        if (is_outdated(config::path::current)) {
            return overwrite(deps);
        }
        return Ok();
    }

    // -------------------- FROM LOCKFILE --------------------

    [[nodiscard]] resolver::UniqDeps<resolver::WithDeps>
    convert_to_deps(const Lockfile& lock) {
        resolver::UniqDeps<resolver::WithDeps> deps;
        for (const auto& package : lock.package) {
            resolver::UniqDeps<resolver::WithDeps>::mapped_type inner_deps = None;
            if (!package.dependencies.empty()) {
                // When serializing lockfile, package version of inner dependencies
                // will be dropped (ref: `convert_to_lock` function).
                // Thus, the version should be restored just as empty string ("").
                resolver::UniqDeps<resolver::WithDeps>::mapped_type::value_type ideps;
                for (const auto& name : package.dependencies) {
                    ideps.push_back({ name, "" });
                }
            }
            deps.emplace(resolver::Package{ package.name, package.version }, inner_deps);
        }
        return deps;
    }

    [[nodiscard]] Result<Option<resolver::UniqDeps<resolver::WithDeps>>>
    read(const fs::path& base_dir) {
        if (!fs::exists(base_dir / lockfile_name)) {
            return Ok(None);
        }

        try {
            const auto lock = toml::parse(base_dir / lockfile_name);
            const auto parsed_lock = toml::get<Lockfile>(lock);
            if (parsed_lock.version != lockfile_version) {
                return Err<Error::InvalidLockfileVersion>(
                    parsed_lock.version
                );
            }
            return Ok(convert_to_deps(parsed_lock));
        } catch (const std::exception& e) {
            return Err<Error::FailedToReadLockfile>(e.what());
        }
    }
} // end namespace

#endif // !POAC_DATA_LOCKFILE_HPP

#ifndef POAC_CORE_STROITE_UTILS_OPTIONS_HPP
#define POAC_CORE_STROITE_UTILS_OPTIONS_HPP

#include <string>
#include <sstream>
#include <vector>
#include <numeric>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include "./absorb.hpp"
#include "../../exception.hpp"
#include "../../naming.hpp"
#include "../../deper/lock.hpp"
#include "../../deper/semver.hpp"
#include "../../../io/file/path.hpp"
#include "../../../io/cli.hpp"
#include "../../../io/file/yaml.hpp"
#include "../../../util/command.hpp"


namespace poac::core::stroite::utils::options {
    struct compile {
        std::string system; // TODO: systemだけ別の管理にして，compiler.hppに，system, std::string optsとして渡したい．
        std::string version_prefix;
        unsigned int cpp_version;
        std::vector<std::string> source_files;
        std::string source_file;
        std::vector<std::string> include_search_path;
        std::vector<std::string> other_args;
        std::vector<std::string> macro_defns;
        boost::filesystem::path base_dir;
        boost::filesystem::path output_root;
    };
    std::string to_string(const compile& c) {
        namespace fs = boost::filesystem;
        using command = util::command;

        command opts;
        opts += c.version_prefix + std::to_string(c.cpp_version);
        opts += "-c";
        opts += accumulate(begin(c.source_files), end(c.source_files), command());
        opts += accumulate(begin(c.include_search_path), end(c.include_search_path), command(),
                [](command acc, auto s) { return acc + ("-I" + s); });
        opts += accumulate(begin(c.other_args), end(c.other_args), command());
        opts += accumulate(begin(c.macro_defns), end(c.macro_defns), command());
        opts += "-o";
        for (const auto& s : c.source_files) {
            auto obj_path = c.output_root / fs::relative(s);
            obj_path.replace_extension("o");
            fs::create_directories(obj_path.parent_path());
            opts += obj_path.string();
        }
        return opts.string();
    }

    struct link {
        std::string system;
        std::string project_name;
        boost::filesystem::path output_root;
        std::vector<std::string> obj_files_path;
        std::vector<std::string> library_search_path;
        std::vector<std::string> static_link_libs;
        std::vector<std::string> library_path;
        std::vector<std::string> other_args;
    };
    std::string to_string(const link& l) {
        using command = util::command;

        command opts;
        opts += accumulate(begin(l.obj_files_path), end(l.obj_files_path), command());
        opts += accumulate(begin(l.library_search_path), end(l.library_search_path), command(),
                           [](command acc, auto s) { return acc + ("-L" + s); });
        opts += accumulate(begin(l.static_link_libs), end(l.static_link_libs), command(),
                           [](command acc, auto s) { return acc + ("-l" + s); });
        opts += accumulate(begin(l.library_path), end(l.library_path), command());
        opts += accumulate(begin(l.other_args), end(l.other_args), command());
        opts += "-o " + (l.output_root / l.project_name).string();
        return opts.string();
    }

    struct static_lib {
        std::string project_name;
        boost::filesystem::path output_root;
        std::vector<std::string> obj_files_path;
    };
    std::string to_string(const static_lib& s) {
        using command = util::command;

        command opts;
        opts += (s.output_root / s.project_name).string() + ".a";
        opts += accumulate(begin(s.obj_files_path), end(s.obj_files_path), command());
        return opts.string();
    }

    struct dynamic_lib {
        std::string system;
        std::string project_name;
        boost::filesystem::path output_root;
        std::vector<std::string> obj_files_path;
    };
    std::string to_string(const dynamic_lib& d) {
        using command = util::command;

        command opts;
        opts += absorb::dynamic_lib_option;
        const std::string extension = absorb::dynamic_lib_extension;
        opts += accumulate(begin(d.obj_files_path), end(d.obj_files_path), command());
        opts += "-o";
        opts += (d.output_root / d.project_name).string() + extension;
        return opts.string();
    }


    template <typename Opts>
    void enable_gnu(Opts& opts) { // TODO:
        opts.version_prefix = "-std=gnu++";
    }

    // TODO: できれば，implに，
    std::string default_version_prefix() {
        return "-std=c++";
    }
    template <typename T>
    std::string make_macro_defn(const std::string& first, const T& second) {
        return make_macro_defn(first, std::to_string(second));
    }
    template <>
    std::string make_macro_defn<std::string>(const std::string& first, const std::string& second) {
        return "-D" + first + "=" + R"(\")" + second + R"(\")";
    }
    template <>
    std::string make_macro_defn<std::uint64_t>(const std::string& first, const std::uint64_t& second) {
        std::ostringstream oss;
        oss << second;
        return make_macro_defn(first, oss.str());
    }

    std::vector<std::string>
    make_macro_defns(const std::map<std::string, YAML::Node>& node) {
        namespace fs = boost::filesystem;
        namespace yaml = io::file::yaml;

        std::vector<std::string> macro_defns;
        // poac automatically define the absolute path of the project's root directory.
        // TODO: これ，依存関係もこれ使ってたら，それも，ルートのにならへん？header-only libの時
        macro_defns.emplace_back(make_macro_defn("POAC_PROJECT_ROOT", fs::current_path().string()));
        const auto version = deper::semver::Version(yaml::get_with_throw<std::string>(node.at("version")));
        macro_defns.emplace_back(make_macro_defn("POAC_VERSION", version.get_full()));
        macro_defns.emplace_back(make_macro_defn("POAC_MAJOR_VERSION", version.major));
        macro_defns.emplace_back(make_macro_defn("POAC_MINOR_VERSION", version.minor));
        macro_defns.emplace_back(make_macro_defn("POAC_PATCH_VERSION", version.patch));
        return macro_defns;
    }

    std::vector<std::string>
    make_include_search_path(const bool exist_deps_key) {
        namespace fs = boost::filesystem;
        namespace lock = deper::lock;
        namespace yaml = io::file::yaml;
        namespace path = io::file::path;

        std::vector<std::string> include_search_path;
        if (exist_deps_key) { // depsキーが存在する // TODO: subcmd/build.hppで，存在確認が取れている
            if (const auto locked_deps = lock::load_ignore_timestamp()) {
                for (const auto& [name, dep] : locked_deps->backtracked) {
                    const std::string current_package_name = naming::to_current(dep.source, name, dep.version);
                    const fs::path include_dir = path::current_deps_dir / current_package_name / "include";

                    if (path::validate_dir(include_dir)) {
                        include_search_path.push_back(include_dir.string());
                    }
                    else {
                        throw exception::error(
                                name + " is not installed.\n"
                                       "Please build after running `poac install`");
                    }
                }
            }
            else {
                throw exception::error(
                        "Could not load poac.lock.\n"
                        "Please build after running `poac install`");
            }
        }
        return include_search_path;
    }

    std::vector<std::string>
    make_compile_other_args(const std::map<std::string, YAML::Node>& node) {
        namespace yaml = io::file::yaml;
        if (const auto compile_args = yaml::get<std::vector<std::string>>(node.at("build"), "compile_args")) {
            return *compile_args;
        }
        else {
            return {};
        }
    }
} // end namespace
#endif // POAC_CORE_STROITE_UTILS_OPTIONS_HPP

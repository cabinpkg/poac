#ifndef POAC_SUBCMD_BUILD_HPP
#define POAC_SUBCMD_BUILD_HPP

#include <iostream>
#include <string>
#include <regex>

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

#include "../core/exception.hpp"
#include "../io/file.hpp"
#include "../io/cli.hpp"
#include "../util/compiler.hpp"
#include "../util/package.hpp"


namespace poac::subcmd { struct build {
    static const std::string summary() { return "Beta: Compile all sources that depend on this project."; }
    static const std::string options() { return "[-v | --verbose]"; } // TODO: --no-cache

    template <typename VS, typename = std::enable_if_t<std::is_rvalue_reference_v<VS&&>>>
    void operator()(VS&& argv) { _main(std::move(argv)); }
    template <typename VS, typename = std::enable_if_t<std::is_rvalue_reference_v<VS&&>>>
    void _main(VS&& argv) {
        namespace fs     = boost::filesystem;
        namespace except = core::exception;

        check_arguments(argv);

        const auto project_name = io::file::yaml::get_node("name").as<std::string>();

        util::compiler compiler;
        configure(compiler, project_name);

        // TODO: --backend cmake
        // TODO: poac.yml compiler: -> error version outdated

        // TODO: name, version, cpp_versionが無い時．
        // TODO: runの時に，build: bin:true なければ，かつ，./main.cppが無ければ，runはエラーになる．
        // TODO: もしくは，main.cppじゃなくても，バイナリを生成するソースをpoac.ymlから指定できるようにしておく．
        // TODO: curl-configのように．
        const bool verbose = (argv.size() > 0 && (argv[0] == "-v" || argv[0] == "--verbose"));
        if (const auto ret = io::file::yaml::get<bool>(io::file::yaml::get_node("build"), "bin"))
            if (*ret) bin_build(compiler, project_name, verbose);
        // TODO: runの時はいらない？？？
        if (const auto ret = io::file::yaml::get<bool>(io::file::yaml::get_node("build"), "lib"))
            if (*ret) lib_build(compiler, project_name, verbose);
    }

    void configure(util::compiler& compiler, const std::string& project_name) {
        namespace fs = boost::filesystem;

        const auto project_version = io::file::yaml::get_node("version").as<std::string>();
        const auto project_cpp_version = io::file::yaml::get_node("cpp_version").as<unsigned int>();


        compiler.project_name = project_name;
        compiler.system = std::getenv("CXX"); // TODO: なかった時のエラーもしくは，自動選択
        compiler.cpp_version = project_cpp_version;
        // TODO: 存在確認
        compiler.main_cpp = "main.cpp";
        for (const fs::path& p : fs::recursive_directory_iterator(fs::current_path() / "src"))
            if (!fs::is_directory(p) && p.extension().string() == ".cpp")
                compiler.add_source_file(p.string());
        compiler.output_path = io::file::path::current_build_bin_dir;

        compiler.add_macro_defn(std::make_pair("POAC_ROOT", std::getenv("PWD")));
        const std::string def_macro_name = boost::to_upper_copy<std::string>(project_name) + "_VERSION";
        compiler.add_macro_defn(std::make_pair(def_macro_name, project_version));

        const auto deps = io::file::yaml::get_node("deps");
        for (YAML::const_iterator itr = deps.begin(); itr != deps.end(); ++itr) {
            const std::string name = itr->first.as<std::string>();
            const std::string src = util::package::get_source(itr->second);
            const std::string version = util::package::get_version(itr->second, src);
            const std::string pkgname = util::package::cache_to_current(util::package::github_conv_pkgname(name, version));
            const fs::path pkgpath = io::file::path::current_deps_dir / pkgname;

            if (const fs::path include_dir = pkgpath / "include"; fs::exists(include_dir))
                compiler.add_include_search_path(include_dir.string());
            if (const fs::path lib_dir = pkgpath / "lib"; fs::exists(lib_dir)) {
                compiler.add_library_search_path(lib_dir.string());

                if (const auto vs = io::file::yaml::get2<std::vector<std::string>>(itr->second, "link", "include"))
                    for (const auto &s : *vs)
                        compiler.add_static_link_lib(s);
                else if (io::file::yaml::exists_key(itr->second, "link"))
                    compiler.add_static_link_lib(pkgname);
            }
        }
        // //lib/x86_64-linux-gnu/libpthread.so.0: error adding symbols: DSO missing from command line
        // TODO: 抽象化
        compiler.add_other_args("-pthread");
    }

    void bin_build(const util::compiler& compiler, const std::string& project_name, const bool verbose=false) {
        namespace fs = boost::filesystem;
        const auto project_path = (io::file::path::current_build_bin_dir / project_name).string();

        fs::create_directories(io::file::path::current_build_bin_dir);
        if (compiler.link(verbose)) {
            std::cout << io::cli::green << "Compiled: " << io::cli::reset
                      << "Output to `" + fs::relative(project_path).string() + "`"
                      << std::endl;
        }
        else { /* error */ // TODO: compileに失敗時も表示されてしまう．
            std::cout << io::cli::yellow << "Warning: " << io::cli::reset
            << "There is no change. Binary exists in `" + fs::relative(project_path).string() + "`."
            << std::endl;
        }
    }
    // Generate link libraries
    void lib_build(const util::compiler& compiler, const std::string& project_name, const bool verbose=false) {
        namespace fs = boost::filesystem;

        // Generate link libraries.
        fs::create_directories(io::file::path::current_build_lib_dir);
        if (compiler.gen_static_lib(verbose)) {
            std::cout << io::cli::green << "Generated: " << io::cli::reset
                      << "Output to `" + fs::relative(io::file::path::current_build_lib_dir / project_name).string() + ".a" + "`"
                      << std::endl;
        }
        else { /* error */
            std::cout << io::cli::yellow << "Warning: " << io::cli::reset
                      << "There is no change. Static library exists in `" + fs::relative(io::file::path::current_build_lib_dir / project_name).string() + ".a" + "`."
                      << std::endl;
        }
        /* TODO: clangやgccのエラーメッセージをパースできるのなら，else文を実装する．
        そうでないのなら，util::commandで，std_errはそのまま垂れ流させて，標準のエラーメッセージを見せた方がわかりやすい．*/
        if (compiler.gen_dynamic_lib(verbose)) {
            std::cout << io::cli::green << "Generated: " << io::cli::reset
                      << "Output to `" + fs::relative(io::file::path::current_build_lib_dir / project_name).string() + ".dylib" + "`"
                      << std::endl;
        }
        else { /* error */
            std::cout << io::cli::yellow << "Warning: " << io::cli::reset
                      << "There is no change. Dynamic library exists in `" + fs::relative(io::file::path::current_build_lib_dir / project_name).string() + ".dylib" + "`."
                      << std::endl;
        }
    }


    void check_arguments(const std::vector<std::string>& argv) {
        namespace except = core::exception;

        if (argv.size() >= 2)
            throw except::invalid_second_arg("build");
    }
};} // end namespace
#endif // !POAC_SUBCMD_BUILD_HPP

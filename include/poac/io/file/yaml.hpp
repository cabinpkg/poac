#ifndef POAC_IO_FILE_YAML_HPP
#define POAC_IO_FILE_YAML_HPP

#include <iostream>
#include <string>
#include <map>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <yaml-cpp/yaml.h>


namespace poac::io::file::yaml {
    boost::optional<std::string> exists_setting_file() {
        namespace fs = boost::filesystem;
        const auto cur = boost::filesystem::current_path();
        if (fs::exists(cur / fs::path("poac.yml")))
            return std::string("poac.yml");
        else if (fs::exists(cur / fs::path("poac.yaml")))
            return std::string("poac.yaml");
        else
            return boost::none;
    }

    boost::optional<YAML::Node> load(const std::string& filename) {
        try { return YAML::LoadFile(filename); }
        catch (...) { return boost::none; }
    }

    template <typename T>
    boost::optional<T> get(const YAML::Node& node) {
        try { return node.as<T>(); }
        catch (...) { return boost::none; }
    }
    template <typename T>
    boost::optional<T> get1(const YAML::Node& node, const std::string& key) {
        try { return node[key].as<T>(); }
        catch (...) { return boost::none; }
    }
    template <typename T>
    boost::optional<T> get2(const YAML::Node& node, const std::string& key1, const std::string& key2) {
        try { return node[key1][key2].as<T>(); }
        catch (...) { return boost::none; }
    }

    // {{"arg1", node["arg1"]}, ...}
    template <typename... Args>
    static boost::optional<std::map<std::string, YAML::Node>>
    get_by_width(const YAML::Node &node, const Args&... args) {
        std::map<std::string, YAML::Node> mp;
        try {
            ((mp[args] = node[args]), ...);
            return mp;
        }
        catch (...) { return boost::none; }
    }
    // node[arg1][arg2]...
    // TODO: 多分，内部がポインタで実装されてて，書き換えると，どこから見ても書き換わってしまう．
    // そのため，この関数には注意して使用する．？？
    template <typename... Args>
    static boost::optional<YAML::Node>
    get_by_depth(YAML::Node node, const Args&... args) {
        try {
            ((node = node[args]), ...);
            return node;
        }
        catch (...) { return boost::none; }
    }
} // end namespace
#endif // !POAC_IO_FILE_YAML_HPP

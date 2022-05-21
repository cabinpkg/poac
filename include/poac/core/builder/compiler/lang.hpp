#ifndef POAC_CORE_BUILDER_COMPILER_LANG_HPP
#define POAC_CORE_BUILDER_COMPILER_LANG_HPP

// internal
#include <poac/poac.hpp>
#include <poac/util/cfg.hpp> // compiler

namespace poac::core::builder::compiler::lang {
    enum class Lang {
        c,
        cxx,
    };

    String
    to_string(Lang lang) {
        switch (lang) {
            case Lang::c:
                return "C";
            case Lang::cxx:
                return "C++";
            default:
                unreachable();
        }
    }

    std::ostream&
    operator<<(std::ostream& os, Lang lang) {
        return (os << to_string(lang));
    }
} // end namespace

#endif // !POAC_CORE_BUILDER_COMPILER_LANG_HPP

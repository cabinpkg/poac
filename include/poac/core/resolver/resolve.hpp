#ifndef POAC_CORE_RESOLVER_RESOLVE_HPP_
#define POAC_CORE_RESOLVER_RESOLVE_HPP_

// std
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>
#include <regex>
#include <sstream>
#include <stack>
#include <tuple>
#include <utility>

// external
#include <boost/dynamic_bitset.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>
#include <boost/range/irange.hpp>
#include <boost/range/join.hpp>
#include <spdlog/spdlog.h> // NOLINT(build/include_order)

// internal
#include "poac/config.hpp"
#include "poac/core/resolver/sat.hpp"
#include "poac/poac.hpp"
#include "poac/util/meta.hpp"
#include "poac/util/net.hpp"
#include "poac/util/semver/semver.hpp"
#include "poac/util/verbosity.hpp"

namespace poac::core::resolver::resolve {

struct WithDeps : std::true_type {};
struct WithoutDeps : std::false_type {};

// Duplicate dependencies should have non-resolved dependencies which contains
// package info having `version` rather than interval generally. We should avoid
// using `std::unordered_map` here so that packages with the same name possibly
// store in DuplicateDeps. Package information does not need dependencies'
// dependencies (meaning that flattened), so the second value of std::pair is
// this type rather than Package and just needs std::string indicating a
// specific version.
template <typename W>
struct DuplicateDeps {};

template <typename W>
using DupDeps = typename DuplicateDeps<W>::type;

struct Package {
  /// Package name
  String name;

  /// Version Requirement
  ///
  /// Sometimes, this is like `1.66.0` or like `>=1.64.0 and <2.0.0`.
  String version_rq;
};

inline bool
operator==(const Package& lhs, const Package& rhs) {
  return lhs.name == rhs.name && lhs.version_rq == rhs.version_rq;
}

usize
hash_value(const Package& p) {
  usize seed = 0;
  boost::hash_combine(seed, p.name);
  boost::hash_combine(seed, p.version_rq);
  return seed;
}

template <>
struct DuplicateDeps<WithoutDeps> {
  using type = Vec<Package>;
};

using Deps = Option<DupDeps<WithoutDeps>>;

template <>
struct DuplicateDeps<WithDeps> {
  using type = Vec<std::pair<Package, Deps>>;
};

template <typename W>
using UniqDeps = std::conditional_t<
    W::value, HashMap<Package, Deps>,
    // <name, ver_req>
    HashMap<String, String>>;

inline const Package&
get_package(const UniqDeps<WithDeps>::value_type& deps) noexcept {
  return deps.first;
}

String
to_binary_numbers(const i32& x, const usize& digit) {
  return format("{:0{}b}", x, digit);
}

// A ∨ B ∨ C
// A ∨ ¬B ∨ ¬C
// ¬A ∨ B ∨ ¬C
// ¬A ∨ ¬B ∨ C
// ¬A ∨ ¬B ∨ ¬C
Vec<Vec<i32>>
multiple_versions_cnf(const Vec<i32>& clause) {
  return boost::irange(0, 1 << clause.size()) // number of combinations
       | boost::adaptors::transformed([&clause](const i32 i) {
           return boost::dynamic_bitset<>(to_binary_numbers(i, clause.size()));
         }) |
         boost::adaptors::filtered([](const boost::dynamic_bitset<>& bs) {
           return bs.count() != 1;
         }) |
         boost::adaptors::transformed(
             [&clause](const boost::dynamic_bitset<>& bs) -> Vec<i32> {
               return boost::irange(usize{0}, bs.size()) |
                      boost::adaptors::transformed([&clause, &bs](const i32 i) {
                        return bs[i] ? clause[i] * -1 : clause[i];
                      }) |
                      util::meta::containerized;
             }
         ) |
         util::meta::containerized;
}

Vec<Vec<i32>>
create_cnf(const DupDeps<WithDeps>& activated) {
  Vec<Vec<i32>> clauses;
  Vec<i32> already_added;

  DupDeps<WithDeps>::const_iterator first = std::cbegin(activated);
  DupDeps<WithDeps>::const_iterator last = std::cend(activated);
  for (i32 i = 0; i < static_cast<i32>(activated.size()); ++i) {
    if (util::meta::find(already_added, i)) {
      continue;
    }

    const auto name_lambda = [&](const auto& x) {
      return get_package(x) == get_package(activated[i]);
    };
    // No other packages with the same name as the package currently pointed to
    // exist
    if (const i64 count = std::count_if(first, last, name_lambda); count == 1) {
      Vec<i32> clause;
      clause.emplace_back(i + 1);
      clauses.emplace_back(clause);

      // index ⇒ deps
      if (!activated[i].second.has_value()) {
        clause[0] *= -1;
        for (const auto& [name, version] : activated[i].second.value()) {
          // It is guaranteed to exist
          clause.emplace_back(
              util::meta::index_of_if(
                  first, last,
                  [&n = name, &v = version](const auto& d) {
                    return get_package(d).name == n &&
                           get_package(d).version_rq == v;
                  }
              ) +
              1
          );
        }
        clauses.emplace_back(clause);
      }
    } else if (count > 1) {
      Vec<i32> clause;

      for (DupDeps<WithDeps>::const_iterator found = first; found != last;
           found = std::find_if(found, last, name_lambda)) {
        const i64 index = std::distance(first, found);
        clause.emplace_back(index + 1);
        already_added.emplace_back(index + 1);

        // index ⇒ deps
        if (!found->second.has_value()) {
          Vec<i32> new_clause;
          new_clause.emplace_back(index);
          for (const Package& package : found->second.value()) {
            // It is guaranteed to exist
            new_clause.emplace_back(
                util::meta::index_of_if(
                    first, last,
                    [&package](const auto& p) {
                      return get_package(p).name == package.name &&
                             get_package(p).version_rq == package.version_rq;
                    }
                ) +
                1
            );
          }
          clauses.emplace_back(new_clause);
        }
        ++found;
      }
      boost::range::push_back(clauses, multiple_versions_cnf(clause));
    }
  }
  return clauses;
}

[[nodiscard]] Result<UniqDeps<WithDeps>, String>
solve_sat(const DupDeps<WithDeps>& activated, const Vec<Vec<i32>>& clauses) {
  // deps.activated.size() == variables
  const Vec<i32> assignments = Try(sat::solve(clauses, activated.size()));
  UniqDeps<WithDeps> resolved_deps{};
  log::debug("SAT");
  for (i32 a : assignments) {
    log::debug("{} ", a);
    if (a > 0) {
      const auto& [package, deps] = activated[a - 1];
      resolved_deps.emplace(package, deps);
    }
  }
  log::debug(0);
  return Ok(resolved_deps);
}

[[nodiscard]] Result<UniqDeps<WithDeps>, String>
backtrack_loop(const DupDeps<WithDeps>& activated) {
  const Vec<Vec<i32>> clauses = create_cnf(activated);
  if (util::verbosity::is_verbose()) {
    for (const Vec<i32>& c : clauses) {
      for (i32 l : c) {
        const auto deps = activated[std::abs(l) - 1];
        const Package package = get_package(deps);
        log::debug("{}-{}: {}, ", package.name, package.version_rq, l);
      }
      log::debug("");
    }
  }
  return solve_sat(activated, clauses);
}

template <typename SinglePassRange>
bool
duplicate_loose(const SinglePassRange& rng) {
  const auto first = std::begin(rng);
  const auto last = std::end(rng);
  return std::find_if(first, last, [&](const auto& x) {
           return std::count_if(first, last, [&](const auto& y) {
                    return get_package(x).name == get_package(y).name;
                  }) > 1;
         }) != last;
}

// Interval to multiple versions
// `>=0.1.2 and <3.4.0` -> { 2.4.0, 2.5.0 }
// name is boost/config, no boost-config
[[nodiscard]] Result<Vec<String>, String>
get_versions_satisfy_interval(const Package& package) {
  // TODO(ken-matsui): (`>1.2 and <=1.3.2` -> NG，`>1.2.0-alpha and <=1.3.2` ->
  // OK) `2.0.0` specific version or `>=0.1.2 and <3.4.0` version interval
  const semver::Interval i(package.version_rq);
  const Vec<String> satisfied_versions =
      Try(util::net::api::versions(package.name)) |
      boost::adaptors::filtered([&i](StringRef s) { return i.satisfies(s); }) |
      util::meta::containerized;

  if (satisfied_versions.empty()) {
    return Err(format(
        "`{}: {}` not found; seem dependencies are broken", package.name,
        package.version_rq
    ));
  }
  return Ok(satisfied_versions);
}

struct Cache {
  Package package;

  /// versions in the interval
  Vec<String> versions;
};

inline bool
operator==(const Cache& lhs, const Cache& rhs) {
  return lhs.package == rhs.package && lhs.versions == rhs.versions;
}

usize
hash_value(const Cache& i) {
  usize seed = 0;
  boost::hash_combine(seed, i.package);
  boost::hash_range(seed, i.versions.begin(), i.versions.end());
  return seed;
}

using IntervalCache = HashSet<Cache>;

inline bool
cache_exists(const IntervalCache& cache, const Package& package) {
  return util::meta::find_if(cache, [&package](const Cache& c) {
    return c.package == package;
  });
}

inline bool
cache_exists(const DupDeps<WithDeps>& deps, const Package& package) {
  return util::meta::find_if(deps, [&package](const auto& c) {
    return get_package(c) == package;
  });
}

DupDeps<WithoutDeps>
gather_deps_of_deps(
    const UniqDeps<WithoutDeps>& deps_api_res, IntervalCache& interval_cache
) {
  DupDeps<WithoutDeps> cur_deps_deps;
  for (const auto& [name, version_rq] : deps_api_res) {
    const Package package{name, version_rq};

    // Check if node package is resolved dependency (by interval)
    const IntervalCache::iterator found_cache =
        boost::range::find_if(interval_cache, [&package](const Cache& cache) {
          return package == cache.package;
        });

    const Vec<String> dep_versions =
        found_cache != interval_cache.cend()
            ? found_cache->versions
            : get_versions_satisfy_interval(package).unwrap();
    if (found_cache == interval_cache.cend()) {
      // Cache interval and versions pair
      interval_cache.emplace(Cache{package, dep_versions});
    }
    for (const String& dep_version : dep_versions) {
      cur_deps_deps.emplace_back(Package{package.name, dep_version});
    }
  }
  return cur_deps_deps;
}

void
gather_deps(
    const Package& package, DupDeps<WithDeps>& new_deps,
    IntervalCache& interval_cache
) {
  // Check if root package resolved dependency (whether the specific version is
  // the same), and check circulating
  if (cache_exists(new_deps, package)) {
    return;
  }

  // Get dependencies of dependencies
  const UniqDeps<WithoutDeps> deps_api_res =
      util::net::api::deps(package.name, package.version_rq).unwrap();
  if (deps_api_res.empty()) {
    new_deps.emplace_back(package, None);
  } else {
    const DupDeps<WithoutDeps> deps_of_deps =
        gather_deps_of_deps(deps_api_res, interval_cache);

    // Store dependency and the dependency's dependencies.
    new_deps.emplace_back(package, deps_of_deps);

    // Gather dependencies of dependencies of dependencies.
    for (const Package& dep_package : deps_of_deps) {
      gather_deps(dep_package, new_deps, interval_cache);
    }
  }
}

[[nodiscard]] Result<DupDeps<WithDeps>, String>
gather_all_deps(const UniqDeps<WithoutDeps>& deps) {
  DupDeps<WithDeps> duplicate_deps;
  IntervalCache interval_cache;

  // Activate the root of dependencies
  for (const auto& [name, version_rq] : deps) {
    const Package package{name, version_rq};

    // Check whether the packages specified in poac.toml
    //   are already resolved which includes
    //   that package's dependencies and package's versions
    //   by checking whether package's interval is the same.
    if (cache_exists(interval_cache, package)) {
      continue;
    }

    // Get versions using interval
    // FIXME: versions API and deps API are received the almost same responses
    const Vec<String> versions = Try(get_versions_satisfy_interval(package));
    // Cache interval and versions pair
    interval_cache.emplace(Cache{package, versions});
    for (const String& version : versions) {
      gather_deps(
          Package{package.name, version}, duplicate_deps, interval_cache
      );
    }
  }
  return Ok(duplicate_deps);
}

} // namespace poac::core::resolver::resolve

#endif // POAC_CORE_RESOLVER_RESOLVE_HPP_

#pragma once

#include "../Rustify.hpp"
#include "Config.hpp"
#include "Global.hpp"
#include "Object.hpp"
#include "Oid.hpp"

#include <git2/clone.h>
#include <git2/repository.h>

namespace git2 {

struct Repository : public GlobalState {
  git_repository* raw = nullptr;

  Repository() = default;
  ~Repository();

  Repository(const Repository&) = default;
  Repository(Repository&&) noexcept = default;
  Repository& operator=(const Repository&) = default;
  Repository& operator=(Repository&&) noexcept = default;

  /// Attempt to open an already-existing repository at `path`.
  ///
  /// The path can point to either a normal or bare repository.
  Repository& open(StringRef path);
  /// Attempt to open an already-existing bare repository at `path`.
  ///
  /// The path can point to only a bare repository.
  Repository& openBare(StringRef path);

  /// Creates a new repository in the specified folder.
  ///
  /// This by default will create any necessary directories to create the
  /// repository, and it will read any user-specified templates when creating
  /// the repository. This behavior can be configured through `init_opts`.
  Repository& init(StringRef path);
  /// Creates a new `--bare` repository in the specified folder.
  ///
  /// The folder must exist prior to invoking this function.
  Repository& initBare(StringRef path);

  /// Check if path is ignored by the ignore rules.
  bool isIgnored(StringRef path) const;

  /// Clone a remote repository.
  Repository&
  clone(StringRef url, StringRef path, const git_clone_options* opts = nullptr);

  /// Find a single object, as specified by a revision string.
  Object revparseSingle(StringRef spec) const;

  /// Make the repository HEAD directly point to the Commit.
  Repository& setHeadDetached(const Oid& oid);

  /// Lookup a reference by name and resolve immediately to OID.
  Oid refNameToId(StringRef refname) const;

  /// Get the configuration file for this repository.
  ///
  /// If a configuration file has not been set, the default config set for
  /// the repository will be returned, including global and system
  /// configurations (if they are available).
  Config config() const;
};

} // end namespace git2

#pragma once

#include "Logger.hpp"
#include "Rustify.hpp"

#include <cstdlib>
#include <iterator>
#include <span>

class Opt;
class Arg;
class Subcmd;
class Command;

// Defined in main.cc
const Command& getCmd() noexcept;

template <typename Derived>
class CliBase {
protected:
  // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)
  StringRef name;
  StringRef desc;
  // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

public:
  constexpr CliBase() noexcept = default;
  constexpr ~CliBase() noexcept = default;
  constexpr CliBase(const CliBase&) noexcept = default;
  constexpr CliBase(CliBase&&) noexcept = default;
  constexpr CliBase& operator=(const CliBase&) noexcept = default;
  constexpr CliBase& operator=(CliBase&&) noexcept = default;

  constexpr explicit CliBase(const StringRef name) noexcept : name(name) {}
  constexpr Derived& setDesc(const StringRef desc) noexcept {
    this->desc = desc;
    return static_cast<Derived&>(*this);
  }
};

template <typename Derived>
class ShortAndHidden {
protected:
  // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)
  StringRef shortName;
  bool isHidden = false;
  // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

public:
  constexpr Derived& setShort(const StringRef shortName) noexcept {
    this->shortName = shortName;
    return static_cast<Derived&>(*this);
  }
  constexpr Derived& setHidden(const bool isHidden) noexcept {
    this->isHidden = isHidden;
    return static_cast<Derived&>(*this);
  }
};

class Opt : public CliBase<Opt>, public ShortAndHidden<Opt> {
  friend class Subcmd;
  friend class Command;

  StringRef placeholder;
  StringRef defaultVal;
  bool isGlobal = false;

public:
  using CliBase::CliBase;

  friend void
  addOptCandidates(Vec<StringRef>& candidates, const Vec<Opt>& opts) noexcept;
  friend usize calcOptMaxShortSize(const Vec<Opt>& opts) noexcept;
  friend usize
  calcOptMaxOffset(const Vec<Opt>& opts, usize maxShortSize) noexcept;
  friend void
  printOpts(const Vec<Opt>& opts, usize maxShortSize, usize maxOffset) noexcept;

  constexpr Opt setPlaceholder(const StringRef placeholder) noexcept {
    this->placeholder = placeholder;
    return *this;
  }
  constexpr Opt setDefault(const StringRef defaultVal) noexcept {
    this->defaultVal = defaultVal;
    return *this;
  }
  constexpr Opt setGlobal(const bool isGlobal) noexcept {
    this->isGlobal = isGlobal;
    return *this;
  }

private:
  /// Size of `-c, --color <WHEN>` without color.
  constexpr usize leftSize(usize maxShortSize) const noexcept {
    // shrt.size() = ?
    // `, `.size() = 2
    // lng.size() = ?
    // ` `.size() = 1
    // placeholder.size() = ?
    return 3 + maxShortSize + name.size() + placeholder.size();
  }

  void print(usize maxShortSize, usize maxOffset) const noexcept;
};

class Arg : public CliBase<Arg> {
  friend class Subcmd;

  bool required = true;
  bool variadic = false;

public:
  using CliBase::CliBase;

  constexpr Arg setRequired(const bool required) noexcept {
    this->required = required;
    return *this;
  }
  constexpr Arg setVariadic(const bool variadic) noexcept {
    this->variadic = variadic;
    return *this;
  }

private:
  /// Size of left side of the help message.
  constexpr usize leftSize() const noexcept {
    return name.size();
  }

  String getLeft() const noexcept;
  void print(usize maxOffset) const noexcept;
};

class Subcmd : public CliBase<Subcmd>, public ShortAndHidden<Subcmd> {
  friend class Command;

  StringRef cmdName;
  Option<Vec<Opt>> globalOpts = None;
  Vec<Opt> localOpts;
  Arg arg;
  Fn<int(std::span<const StringRef>)> mainFn;

public:
  using CliBase::CliBase;

  constexpr Subcmd& setArg(Arg arg) noexcept {
    this->arg = arg;
    return *this;
  }

  Subcmd& addOpt(Opt opt) noexcept;
  Subcmd& setMainFn(Fn<int(std::span<const StringRef>)> mainFn) noexcept;
  [[nodiscard]] int noSuchArg(StringRef arg) const;

private:
  constexpr bool hasShort() const noexcept {
    return !shortName.empty();
  }
  constexpr Subcmd& setCmdName(StringRef cmdName) noexcept {
    this->cmdName = cmdName;
    return *this;
  }

  Subcmd& setGlobalOpts(const Vec<Opt>& globalOpts) noexcept;
  String getUsage() const noexcept;
  void printHelp() const noexcept;
  void print(usize maxOffset) const noexcept;

  usize calcMaxShortSize() const noexcept;
  /// Calculate the maximum length of the left side of the helps to align the
  /// descriptions with 2 spaces.
  usize calcMaxOffset(usize maxShortSize) const noexcept;
};

class Command : public CliBase<Command> {
  HashMap<StringRef, Subcmd> subcmds;
  Vec<Opt> globalOpts;
  Vec<Opt> localOpts;

public:
  using CliBase::CliBase;

  Command& addSubcmd(const Subcmd& subcmd) noexcept;
  Command& addOpt(Opt opt) noexcept;
  bool hasSubcmd(StringRef subcmd) const noexcept;

  [[nodiscard]] int noSuchArg(StringRef arg) const;
  [[nodiscard]] int
  exec(StringRef subcmd, std::span<const StringRef> args) const;
  void printSubcmdHelp(StringRef subcmd) const noexcept;
  [[nodiscard]] int printHelp(std::span<const StringRef> args) const noexcept;
  usize calcMaxOffset(usize maxShortSize) const noexcept;

  void printAllSubcmds(bool showHidden, usize maxOffset = 0) const noexcept;

  // Returns the exit code if the global option was handled and needs to be
  // terminated, otherwise None.
  // TODO: result-like types make more sense.
  [[nodiscard]] static inline Option<int> handleGlobalOpts(
      std::forward_iterator auto& itr, std::forward_iterator auto end,
      StringRef subcmd
  ) {
    if (*itr == "-h"sv || *itr == "--help"sv) {
      if (!subcmd.empty()) {
        // {{ }} is a workaround for std::span until C++26
        return getCmd().printHelp({ { subcmd } });
      } else {
        return getCmd().printHelp({});
      }
    } else if (*itr == "-v"sv || *itr == "--verbose"sv) {
      logger::setLevel(logger::Level::Debug);
    } else if (*itr == "-vv"sv) {
      logger::setLevel(logger::Level::Trace);
    } else if (*itr == "-q"sv || *itr == "--quiet"sv) {
      logger::setLevel(logger::Level::Off);
    } else if (*itr == "--color"sv) {
      if (itr + 1 < end) {
        setColorMode(*++itr);
      } else {
        logger::error("missing argument for `--color`");
        return EXIT_FAILURE;
      }
    }
    return None;
  }

private:
  usize calcMaxShortSize() const noexcept;

  /// Print help message for poac itself.
  void printCmdHelp() const noexcept;
};

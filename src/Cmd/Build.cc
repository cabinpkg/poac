#include "Build.hpp"

#include "../BuildConfig.hpp"

#include <cstdlib>
#include <iostream>

int build(Vec<String> args) {
  emitMakefile(args);

  std::system(("cd " + OUT_DIR + " && make").c_str());
  return EXIT_SUCCESS;
}

void buildHelp() {
  std::cout << "poac-build" << '\n';
  std::cout << '\n';
  std::cout << "USAGE:" << '\n';
  std::cout << "    poac build [OPTIONS]" << '\n';
  std::cout << '\n';
  std::cout << "OPTIONS:" << '\n';
  std::cout << "    -d, --debug\t\tBuild with debug information (default)"
            << '\n';
  std::cout << "    -r, --release\tBuild with optimizations" << '\n';
}

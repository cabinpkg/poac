include_guard(GLOBAL)

message(CHECK_START "Adding git2-cpp")
list(APPEND CMAKE_MESSAGE_INDENT "  ")

FetchContent_Declare(
        git2-cpp
        GIT_REPOSITORY https://github.com/ken-matsui/git2-cpp.git
        GIT_TAG        0.1.1
)

set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(git2-cpp)
unset(BUILD_SHARED_LIBS)

# target: git2-cpp::git2-cpp
message(CHECK_PASS "added")

list(POP_BACK CMAKE_MESSAGE_INDENT)

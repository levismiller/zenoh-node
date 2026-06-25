# Replaces the bare 'cargo' invocation in zenoh-c's CMakeLists.txt with the
# full path found at cmake configure time, so the build step does not rely on
# cargo being in PATH (which is not guaranteed when run via a package manager).
file(READ "${SOURCE_DIR}/CMakeLists.txt" CONTENT)
string(REPLACE " cargo " " ${CARGO} " CONTENT "${CONTENT}")
file(WRITE "${SOURCE_DIR}/CMakeLists.txt" "${CONTENT}")


-- build src/CMakeFiles/dyn.dir/dynArray.c.o: C_COMPILER /home/joe/private/work/dyn/src/dynArray.c
--   DEP_FILE = src/CMakeFiles/dyn.dir/dynArray.c.o.d
--   FLAGS = -I/home/joe/private/work/dyn/src    -g
--   OBJECT_DIR = src/CMakeFiles/dyn.dir
-- build src/CMakeFiles/dyn.dir/dynMap.c.o: C_COMPILER /home/joe/private/work/dyn/src/dynMap.c
--   DEP_FILE = src/CMakeFiles/dyn.dir/dynMap.c.o.d
--   FLAGS = -I/home/joe/private/work/dyn/src    -g
--   OBJECT_DIR = src/CMakeFiles/dyn.dir
-- build src/CMakeFiles/dyn.dir/dynString.c.o: C_COMPILER /home/joe/private/work/dyn/src/dynString.c
--   DEP_FILE = src/CMakeFiles/dyn.dir/dynString.c.o.d
--   FLAGS = -I/home/joe/private/work/dyn/src    -g
--   OBJECT_DIR = src/CMakeFiles/dyn.dir
-- 
-- # =============================================================================
-- # Link build statements for STATIC_LIBRARY target dyn
-- 
-- 
-- #############################################
-- # Link the static library src/libdyn.a
-- 
-- build src/libdyn.a: C_STATIC_LIBRARY_LINKER src/CMakeFiles/dyn.dir/dynArray.c.o src/CMakeFiles/dyn.dir/dynMap.c.o src/CMakeFiles/dyn.dir/dynString.c.o
--   POST_BUILD = :
--   PRE_LINK = :
--   TARGET_PDB = dyn.a.dbg

function add_library(target, ...)
    sources = { ... }
    for i,src in ipairs(sources) do
        dst = interp("{BASENAME}.o", { path=src, root=DCM_CURRENT_BINARY_DIR })
        add_target(DEFAULT_PLATFORM, "object", dst, { src }, {})
    end
--    add_target(DEFAULT_PLATFORM, "library", target, { ... })
end

-- stuff that needs to go in a toolchain file

rule("default", "object", {
    depfile = "$DEP_FILE",
    command = "/usr/bin/cc $DEFINES $FLAGS -MMD -MT $out -MF \"$DEP_FILE\" -o $out   -c $in",
    description = "Building C object $out"
})

rule("default", "library", {
    command = "$PRE_LINK && /home/joe/private/work/cmake-2.8.10.2/bin/cmake -E remove $out && /usr/bin/ar cr $out $LINK_FLAGS $in && /usr/bin/ranlib $out && $POST_BUILD",
    description = "Linking C static library $out"
})

DEFAULT_PLATFORM = "default"

-- rule CUSTOM_COMMAND
--   command = $COMMAND
--   description = $DESC
--   restat = 1
-- #############################################
-- # Rule for compiling C files.
-- 
-- rule C_COMPILER
--   depfile = $DEP_FILE
--   command = /usr/bin/cc  $DEFINES $FLAGS -MMD -MT $out -MF "$DEP_FILE" -o $out   -c $in
--   description = Building C object $out
-- 
-- 
-- #############################################
-- # Rule for linking C static library.
-- 
-- rule C_STATIC_LIBRARY_LINKER
--   command = $PRE_LINK && /home/joe/private/work/cmake-2.8.10.2/bin/cmake -E remove $out && /usr/bin/ar cr $out $LINK_FLAGS $in && /usr/bin/ranlib $out && $POST_BUILD
--   description = Linking C static library $out
-- 
-- 
-- #############################################
-- # Rule for linking C static library.
-- 
-- rule C_STATIC_LIBRARY_LINKER_RSP_FILE
--   command = $PRE_LINK && /home/joe/private/work/cmake-2.8.10.2/bin/cmake -E remove $out && /usr/bin/ar cr $out $LINK_FLAGS @$RSP_FILE && /usr/bin/ranlib $out && $POST_BUILD
--   description = Linking C static library $out
--   rspfile = $RSP_FILE
--   rspfile_content = $in  $LINK_PATH $LINK_LIBRARIES
-- 
-- 
-- #############################################
-- # Rule for linking C executable.
-- 
-- rule C_EXECUTABLE_LINKER
--   command = $PRE_LINK && /usr/bin/cc  $FLAGS  $LINK_FLAGS $in  -o $out $LINK_PATH $LINK_LIBRARIES && $POST_BUILD
--   description = Linking C executable $out
-- 
-- 
-- #############################################
-- # Rule for linking C executable.
-- 
-- rule C_EXECUTABLE_LINKER_RSP_FILE
--   command = $PRE_LINK && /usr/bin/cc  $FLAGS  $LINK_FLAGS @$RSP_FILE  -o $out  && $POST_BUILD
--   description = Linking C executable $out
--   rspfile = $RSP_FILE
--   rspfile_content = $in  $LINK_PATH $LINK_LIBRARIES
-- 
-- 
-- #############################################
-- # Rule for re-running cmake.
-- 
-- rule RERUN_CMAKE
--   command = /home/joe/private/work/cmake-2.8.10.2/bin/cmake -H/home/joe/private/work/dyn -B/home/joe/private/work/dynninja
--   description = Re-running CMake...
--   generator = 1
-- 
-- 
-- #############################################
-- # Rule for cleaning all built files.
-- 
-- rule CLEAN
--   command = /home/joe/bin/ninja -t clean
--   description = Cleaning all built files...
-- 
-- 
-- #############################################
-- # Rule for printing all primary targets available.
-- 
-- rule HELP
--   command = /home/joe/bin/ninja -t targets
--   description = All primary targets available:
-- 






----------------------------------------------------------------------------------------
-- Basic argument checking / parsing

if #dcm.args < 2 then
    dcm.die("dcm requires a source dir and a destination dir!")
end

srcDir = dcm.canonicalize(dcm.args[1], dcm.cwd)
dstDir = dcm.canonicalize(dcm.args[2], dcm.cwd)

toolchain = nil
if #dcm.args > 2 then
    toolchain = dcm.canonicalize(dcm.args[3], dcm.cwd)
end

----------------------------------------------------------------------------------------
-- Wrap the global environment with dcm while we execute dcmBase, so that
-- the Makefile scripting environment only has the globals that were explicitly added.

local _G = _G
setmetatable(dcm, { __index = _G })
setfenv(1, dcm)

-- Sets up what we're planning to scope
scopes = { _G }
scoped = {}

-- vars to store output information
rules = {}
builds = {}
makefiles = {}
phony = {}

function add_scoped(k, v)
    table.insert(scoped, k)
    _G[k] = v
end

function deepcopy(orig)
    local orig_type = type(orig)
    local copy
    if orig_type == 'table' then
        copy = {}
        for orig_key, orig_value in next, orig, nil do
            copy[deepcopy(orig_key)] = deepcopy(orig_value)
        end
        setmetatable(copy, deepcopy(getmetatable(orig)))
    else -- number, string, boolean, etc
        copy = orig
    end
    return copy
end

function updateScope()
    local currScope = dcm.scopes[ #dcm.scopes ]
    _G.DCM_CURRENT_SRC = currScope.DCM_CURRENT_SRC
    _G.DCM_CURRENT_DST = currScope.DCM_CURRENT_DST
    for i,v in ipairs(dcm.scoped) do
        _G[v] = currScope[v]
    end
end

function pushScope(dir)
    if dir == nil then
        dir = "."
    end
    local prevScope = dcm.scopes[ #dcm.scopes ]
    local newScope = {
        DCM_CURRENT_SRC = dcm.canonicalize(dir, prevScope.DCM_CURRENT_SRC),
        DCM_CURRENT_DST = dcm.canonicalize(dir, prevScope.DCM_CURRENT_DST),
    }
    for i,v in ipairs(dcm.scoped) do
        newScope[v] = deepcopy(prevScope[v])
    end
    table.insert(dcm.scopes, newScope)
    updateScope()
end

function popScope()
    table.remove(dcm.scopes)
    updateScope()
end

function join(t, sep, prefix)
    local text = ""
    for i,v in ipairs(t) do
        if i ~= 1 then
            text = text .. sep
        end
        if prefix then
            text = text .. prefix
        end
        text = text .. v
    end
    return text
end

function output()
    add_target("default", "dcm", "build.ninja", {}, dcm.makefiles, nil, {})

    local b = ""

    for rulename,vars in pairs(rules) do
        b = b .. "\nrule " .. rulename .. "\n"
        for k,v in pairs(vars) do
            b = b .. "  " .. k .. " = " .. v .. "\n"
        end
    end
    for i,v in ipairs(builds) do
        b = b .. v
    end

    local all = "build all: phony"
    for phonyname, phonylist in pairs(phony) do
        b = b .. "\nbuild " .. phonyname .. ": phony"
        for i,v in ipairs(phonylist) do
            b = b .. " " .. v
            all = all .. " " .. v
        end
        b = b .. "\n"
    end

    b = b .. all .. "\n\ndefault all\n"

    local filename = dcm.canonicalize("build.ninja", dcm.dstDir)
    print("Writing " .. filename)
    dcm.write(filename, b)
end

----------------------------------------------------------------------------------------
-- External dcm.* functions

function include_directories(...)
    local t = { ... }
    for i,v in ipairs(t) do
        table.insert(_G.INCLUDES, dcm.canonicalize(v, DCM_CURRENT_SRC))
    end
end

function add_definitions(...)
    local t = { ... }
    for i,v in ipairs(t) do
        table.insert(_G.DEFINES, v)
    end
end

function remove_definitions(...)
    local args = { ... }
    local badDefs = {}
    for i,v in ipairs(args) do
        badDefs[v] = 1
    end

    local newDefs = {}
    local t = { ... }
    for i,v in ipairs(t) do
        if not badDefs[v] then
            table.insert(newDefs, v)
        end
    end
    _G.DEFINES = newDefs
end

function add_subdirectory(dir)
    dcm.pushScope(dir)
    local makefile = dcm.canonicalize("Makefile.lua", DCM_CURRENT_SRC)
    table.insert(dcm.makefiles, makefile)
    print("Reading " .. makefile)
    local text = dcm.read(makefile)
    if text then
        assert(loadstring(text, makefile))()
    else
        dcm.die("Cannot read " .. makefile)
    end
    dcm.popScope()
end

function add_target(platform, rule, dst, srcExplicit, srcImplicit, srcOrderOnly, vars)
    local build = "\nbuild " .. dst ..  ": " .. platform .. "_" .. rule .. " "
    if srcExplicit then
        for i,v in ipairs(srcExplicit) do
            build = build .. " " .. v
        end
    end
    if srcImplicit then
        build = build .. " | "
        for i,v in ipairs(srcImplicit) do
            build = build .. " " .. v
        end
    end
    if srcOrderOnly then
        build = build .. " || "
        for i,v in ipairs(srcOrderOnly) do
            build = build .. " " .. v
        end
    end
    build = build .. "\n"
    for k,v in pairs(vars) do
        build = build .. "    ".. k .. " = " .. tostring(v) .. "\n"
    end

    table.insert(dcm.builds, build)
end

function rule(platform, name, vars)
    dcm.rules[platform .. "_" .. name] = vars
end

function add_library(target, ...)
    sources = { ... }
    objects = {}
    for i,s in ipairs(sources) do
        local src = dcm.canonicalize(s, DCM_CURRENT_SRC)
        local dst = interp("{BASENAME}.o", { path=src, root=DCM_CURRENT_DST })
        local flags = ""
        add_target(DEFAULT_PLATFORM, "object", dst, { src }, nil, nil, {
            DEP_FILE = dst .. ".d",
            FLAGS = dcm.join(INCLUDES, " ", "-I") .. " " .. dcm.join(DEFINES, " ", "")
        })
        table.insert(objects, dst)
    end
    local dst = dcm.canonicalize(target .. ".a", DCM_CURRENT_DST)
    add_target(DEFAULT_PLATFORM, "library", dst, objects, nil, nil, {
        LINK_FLAGS = dcm.join(LINK_FLAGS, " ", "")
    })
    if not phony[target] then
        phony[target] = {}
    end
    table.insert(phony[target], dst)
end

----------------------------------------------------------------------------------------
-- Restore the original global environment

setfenv(1, _G)

----------------------------------------------------------------------------------------
-- Aliases

add_definitions     = dcm.add_definitions
remove_definitions  = dcm.remove_definitions
include_directories = dcm.include_directories
add_subdirectory    = dcm.add_subdirectory
add_library         = dcm.add_library
rule                = dcm.rule

----------------------------------------------------------------------------------------
-- Toolchain Defaults

DEFAULT_PLATFORM = "default"

rule("default", "dcm", {
    command = "cd " .. dcm.cwd .. " && "..dcm.cmd.." " .. dcm.join(dcm.args, " "),
    description = "Re-running DCM...",
    generator = "1",
})

rule("default", "object", {
    depfile = "$DEP_FILE",
    command = "/usr/bin/cc $DEFINES $FLAGS -MMD -MT $out -MF \"$DEP_FILE\" -o $out   -c $in",
    description = "Building C object $out"
})

rule("default", "library", {
    command = "rm -f $out && /usr/bin/ar cr $out $LINK_FLAGS $in",
    description = "Linking C static library $out"
})

----------------------------------------------------------------------------------------
-- Read Toolchain

if dcm.toolchain then
    table.insert(dcm.makefiles, dcm.toolchain)
    print("Reading toolchain: " .. dcm.toolchain)
    local toolchain = dcm.read(dcm.toolchain)
    if toolchain then
        assert(loadstring(toolchain, dcm.toolchain))()
    else
        dcm.die("Cannot read " .. dcm.toolchain)
    end
end

----------------------------------------------------------------------------------------
-- Main

-- set up the base scope
DCM_CURRENT_SRC = dcm.srcDir
DCM_CURRENT_DST = dcm.dstDir
dcm.add_scoped("INCLUDES", {})
dcm.add_scoped("DEFINES", {})
dcm.add_scoped("LINK_FLAGS", {})

-- read in the root Makefile
dcm.add_subdirectory(".")

-- generate!
dcm.output()

----------------------------------------------------------------------------------------
-- Makefile contents

--project "dyn"
--
--include_directories "src"
--
--if UNIX then
--    add_definitions "-g"
--end
--
--if WIN32 then
--    add_definitions "/wd4996"
--end
--
--add_subdirectory "src"


--math.sin(1);
--
----dcm.interp();
--
--a = dcm.canonicalize("foo", "/home/joe");
--print("a: " .. a)
--if dcm.UNIX then
--    print("UNIX\n")
--end
--if dcm.WIN32 then
--    print("WIN32\n")
--end
--
--for i,v in ipairs(dcm.args) do
--    print("arg "..i..": "..v)
--end
--
--print("cwd: " .. dcm.cwd);
--
---- build src/CMakeFiles/dyn.dir/dynArray.c.o: C_COMPILER /home/joe/private/work/dyn/src/dynArray.c
----   DEP_FILE = src/CMakeFiles/dyn.dir/dynArray.c.o.d
----   FLAGS = -I/home/joe/private/work/dyn/src    -g
----   OBJECT_DIR = src/CMakeFiles/dyn.dir
---- build src/CMakeFiles/dyn.dir/dynMap.c.o: C_COMPILER /home/joe/private/work/dyn/src/dynMap.c
----   DEP_FILE = src/CMakeFiles/dyn.dir/dynMap.c.o.d
----   FLAGS = -I/home/joe/private/work/dyn/src    -g
----   OBJECT_DIR = src/CMakeFiles/dyn.dir
---- build src/CMakeFiles/dyn.dir/dynString.c.o: C_COMPILER /home/joe/private/work/dyn/src/dynString.c
----   DEP_FILE = src/CMakeFiles/dyn.dir/dynString.c.o.d
----   FLAGS = -I/home/joe/private/work/dyn/src    -g
----   OBJECT_DIR = src/CMakeFiles/dyn.dir
---- 
---- # =============================================================================
---- # Link build statements for STATIC_LIBRARY target dyn
---- 
---- 
---- #############################################
---- # Link the static library src/libdyn.a
---- 
---- build src/libdyn.a: C_STATIC_LIBRARY_LINKER src/CMakeFiles/dyn.dir/dynArray.c.o src/CMakeFiles/dyn.dir/dynMap.c.o src/CMakeFiles/dyn.dir/dynString.c.o
----   POSTdcmUILD = :
----   PRE_LINK = :
----   TARGET_PDB = dyn.a.dbg
--
--function add_library(target, ...)
--    sources = { ... }
--    for i,src in ipairs(sources) do
--        dst = interp("{BASENAME}.o", { path=src, root=DCM_CURRENTdcmINARY_DIR })
--        add_target(DEFAULT_PLATFORM, "object", dst, { src }, {})
--    end
----    add_target(DEFAULT_PLATFORM, "library", target, { ... })
--end
--
-- stuff that needs to go in a toolchain file

--rule("default", "object", {
--    depfile = "$DEP_FILE",
--    command = "/usr/bin/cc $DEFINES $FLAGS -MMD -MT $out -MF \"$DEP_FILE\" -o $out   -c $in",
--    description = "Building C object $out"
--})
--
--rule("default", "library", {
--    command = "$PRE_LINK && /home/joe/private/work/cmake-2.8.10.2/bin/cmake -E remove $out && /usr/bin/ar cr $out $LINK_FLAGS $in && /usr/bin/ranlib $out && $POSTdcmUILD",
--    description = "Linking C static library $out"
--})

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
--   command = $PRE_LINK && /home/joe/private/work/cmake-2.8.10.2/bin/cmake -E remove $out && /usr/bin/ar cr $out $LINK_FLAGS $in && /usr/bin/ranlib $out && $POSTdcmUILD
--   description = Linking C static library $out
-- 
-- 
-- #############################################
-- # Rule for linking C static library.
-- 
-- rule C_STATIC_LIBRARY_LINKER_RSP_FILE
--   command = $PRE_LINK && /home/joe/private/work/cmake-2.8.10.2/bin/cmake -E remove $out && /usr/bin/ar cr $out $LINK_FLAGS @$RSP_FILE && /usr/bin/ranlib $out && $POSTdcmUILD
--   description = Linking C static library $out
--   rspfile = $RSP_FILE
--   rspfile_content = $in  $LINK_PATH $LINK_LIBRARIES
-- 
-- 
-- #############################################
-- # Rule for linking C executable.
-- 
-- rule C_EXECUTABLE_LINKER
--   command = $PRE_LINK && /usr/bin/cc  $FLAGS  $LINK_FLAGS $in  -o $out $LINK_PATH $LINK_LIBRARIES && $POSTdcmUILD
--   description = Linking C executable $out
-- 
-- 
-- #############################################
-- # Rule for linking C executable.
-- 
-- rule C_EXECUTABLE_LINKER_RSP_FILE
--   command = $PRE_LINK && /usr/bin/cc  $FLAGS  $LINK_FLAGS @$RSP_FILE  -o $out  && $POSTdcmUILD
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






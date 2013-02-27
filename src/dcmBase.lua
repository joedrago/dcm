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
libpaths = {}

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
    add_target("default", "dcm", "build.ninja", "build.ninja", {}, dcm.makefiles, nil, {})

    local b = ""

    for rulename,vars in pairs(rules) do
        b = b .. "\nrule " .. rulename .. "\n"
        for k,v in pairs(vars) do
            b = b .. "  " .. k .. " = " .. v .. "\n"
        end
    end
    for name,build in pairs(builds) do
        b = b .. build.header
        for k,v in pairs(build.vars) do
            b = b .. "    ".. k .. " = " .. tostring(v) .. "\n"
        end
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

function add_target(platform, rule, target, dst, srcExplicit, srcImplicit, srcOrderOnly, vars)
    local header = "\nbuild " .. dst ..  ": " .. platform .. "_" .. rule .. " "
    if srcExplicit then
        for i,v in ipairs(srcExplicit) do
            header = header .. " " .. v
        end
    end
    if srcImplicit then
        header = header .. " | "
        for i,v in ipairs(srcImplicit) do
            header = header .. " " .. v
        end
    end
    if srcOrderOnly then
        header = header .. " || "
        for i,v in ipairs(srcOrderOnly) do
            header = header .. " " .. v
        end
    end
    header = header .. "\n"

    dcm.builds[target] = { header=header, vars=vars }
end

function rule(platform, name, vars)
    dcm.rules[platform .. "_" .. name] = vars
end

function add_linked(rulename, absPath, target, ...)
    sources = { ... }
    objects = {}
    if rulename == "library" then
        libpaths[target] = DCM_CURRENT_DST
    end
    for i,s in ipairs(sources) do
        local src = dcm.canonicalize(s, DCM_CURRENT_SRC)
        local dst = interp("{BASENAME}.o", { path=src, root=DCM_CURRENT_DST })
        local flags = ""
        add_target(DCM_DEFAULT_PLATFORM, "object", dst, dst, { src }, nil, nil, {
            DEP_FILE = dst .. ".d",
            FLAGS = dcm.join(INCLUDES, " ", "-I") .. " " .. dcm.join(DEFINES, " ", "")
        })
        table.insert(objects, dst)
    end
    local dst = absPath
    add_target(DCM_DEFAULT_PLATFORM, rulename, target, dst, objects, nil, nil, {
        LINK_FLAGS = dcm.join(LINK_FLAGS, " ", "")
    })
    if not phony[target] then
        phony[target] = {}
    end
    table.insert(phony[target], dst)
end

function add_library(target, ...)
    local dst = dcm.canonicalize("lib" .. target .. ".a", DCM_CURRENT_DST)
    return add_linked("library", dst, target, ...)
end

function add_executable(target, ...)
    local dst = dcm.canonicalize(target, DCM_CURRENT_DST)
    return add_linked("executable", dst, target, ...)
end

function target_link_libraries(target, ...)
    local dst = dcm.canonicalize(target .. "", DCM_CURRENT_DST)
    if not builds[target] then
        dcm.die("target_link_libraries(): no target '"..target.."'")
    end

    if not builds[target].vars.LINK_LIBRARIES then
        builds[target].vars.LINK_LIBRARIES = ""
    end

    local libs = { ... }
    for i,v in ipairs(libs) do
        if libpaths[v] then
            builds[target].vars.LINK_LIBRARIES = builds[target].vars.LINK_LIBRARIES .. " -L" .. libpaths[v]
        end
        builds[target].vars.LINK_LIBRARIES = builds[target].vars.LINK_LIBRARIES .. " -l" .. v
    end
end

----------------------------------------------------------------------------------------
-- Restore the original global environment

setfenv(1, _G)

----------------------------------------------------------------------------------------
-- Aliases

add_definitions       = dcm.add_definitions
remove_definitions    = dcm.remove_definitions
include_directories   = dcm.include_directories
add_subdirectory      = dcm.add_subdirectory
add_library           = dcm.add_library
add_executable        = dcm.add_executable
target_link_libraries = dcm.target_link_libraries
rule                  = dcm.rule

----------------------------------------------------------------------------------------
-- Toolchain Defaults

DCM_DEFAULT_PLATFORM = "linux_gcc"

rule("linux_gcc", "dcm", {
    command = "cd " .. dcm.cwd .. " && "..dcm.cmd.." " .. dcm.join(dcm.args, " "),
    description = "Re-running DCM...",
    generator = "1",
})

rule("linux_gcc", "object", {
    depfile = "$DEP_FILE",
    command = "/usr/bin/cc $DEFINES $FLAGS -MMD -MT $out -MF \"$DEP_FILE\" -o $out   -c $in",
    description = "Building C object $out"
})

rule("linux_gcc", "library", {
    command = "rm -f $out && /usr/bin/ar cr $out $LINK_FLAGS $in",
    description = "Linking C static library $out"
})

rule("linux_gcc", "executable", {
    command = "/usr/bin/cc $FLAGS $LINK_FLAGS $in -o $out $LINK_PATH $LINK_LIBRARIES",
    description = "Linking C executable $out"
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

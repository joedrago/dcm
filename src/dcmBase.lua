function add_library(target, ...)
    add_target(DEFAULT_PLATFORM, "library", target, { ... })
end

-- stuff that needs to go in a toolchain file
if UNIX then
    DEFAULT_PLATFORM = "unix"
end
if WIN32 then
    DEFAULT_PLATFORM = "win32"
end

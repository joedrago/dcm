function ddinclude_directories()
    print("include_directories called", { e="eee", f="wat", g={complicated="structure"} })
end

function add_library(target, ...)
    print("add_library called:", ...)
end

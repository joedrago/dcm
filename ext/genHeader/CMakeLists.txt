add_executable(genHeader genHeader.c)

MACRO(genHeader _list _input _output _namespace)
    GET_FILENAME_COMPONENT(_inputAbsolutePath ${_input} ABSOLUTE)
    GET_FILENAME_COMPONENT(_outputAbsolutePath ${_output} ABSOLUTE)

    add_custom_command(
       OUTPUT ${_outputAbsolutePath}
       COMMAND genHeader
       ARGS
       -i "${_inputAbsolutePath}" -o "${_outputAbsolutePath}" -n ${_namespace}
       DEPENDS ${_input} genHeader
    )
    LIST(APPEND ${_list} ${_outputAbsolutePath})
ENDMACRO(genHeader)

execute_process(
    COMMAND "${WATCH_EXECUTABLE}" --file "${CAPTURE_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "Replay failed (${result}): ${error_output}")
endif()

foreach(expected IN ITEMS
    "WindowOpened 55f003fc2bd0 thunar workspace=1"
    "WindowFocused 55f003fc2bd0"
    "WindowMoved 55f003fc2bd0 workspace=2"
    "WorkspaceFocused 2"
    "WindowTitleChanged 55f003fc2bd0 title=Home, Documents"
    "WindowFloatingChanged 55f003fc2bd0 floating=true"
    "WorkspaceCreated 3"
    "WorkspaceDestroyed 1"
)
    string(FIND "${output}" "${expected}" found_at)
    if(found_at EQUAL -1)
        message(FATAL_ERROR "Replay output missing '${expected}'. Output: ${output}")
    endif()
endforeach()

string(REGEX MATCHALL "WindowFocused 55f003fc2bd0" focus_events "${output}")
list(LENGTH focus_events focus_count)
if(NOT focus_count EQUAL 1)
    message(FATAL_ERROR "Expected one semantic focus event, saw ${focus_count}")
endif()

if(NOT ANDROID)
    message(FATAL_ERROR "This verification file is Android-only")
endif()

if(QUINTUM_ENABLE_NAT_MAPPING)
    message(FATAL_ERROR "Desktop NAT mapping must be disabled on Android")
endif()

if(NOT TARGET quintum_core)
    message(FATAL_ERROR "quintum_core target missing")
endif()

if(NOT TARGET quintumd)
    message(FATAL_ERROR "quintumd target missing")
endif()

message(STATUS "QUINTUM Android core targets configured")

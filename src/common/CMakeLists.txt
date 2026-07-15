# Shared package-diff logic used by both the KCM and the notifier.
add_library(bootcupdatescommon STATIC packagediff.cpp)

target_link_libraries(bootcupdatescommon PUBLIC
    Qt6::Core
    Qt6::Concurrent
    KF6::I18n
)

target_include_directories(bootcupdatescommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Needed because this static lib is linked into shared modules (the KCM).
set_target_properties(bootcupdatescommon PROPERTIES POSITION_INDEPENDENT_CODE ON)

cmake_minimum_required(VERSION 3.16)
project(NCursesUI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library source files (currently header-only)
set(UI_SOURCES
    src/Widget.cpp
    src/Window.cpp
    src/TextInput.cpp
    src/Menu.cpp
    src/Label.cpp
    src/ListBox.cpp
    src/MessageBox.cpp
)

# Library header files
set(UI_HEADERS
    include/ui/Widget.h
    include/ui/Window.h
    include/ui/TextInput.h
    include/ui/Menu.h
    include/ui/Label.h
    include/ui/ListBox.h
    include/ui/MessageBox.h
    include/ui/Types.h
)

# Create the library
add_library(ncurses_ui STATIC ${UI_SOURCES} ${UI_HEADERS})

# Set include directories for the library
target_include_directories(ncurses_ui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link against ncurses
find_package(Curses REQUIRED)
target_link_libraries(ncurses_ui PUBLIC ${CURSES_LIBRARIES})
target_include_directories(ncurses_ui PUBLIC ${CURSES_INCLUDE_DIRS})

# Installation rules (optional, for when you want to install the library)
install(TARGETS ncurses_ui
    EXPORT NCursesUITargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ui
    DESTINATION include
)

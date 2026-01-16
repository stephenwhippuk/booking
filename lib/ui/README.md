# NCurses UI Component Library

A reusable, modular UI component library for building terminal user interfaces with ncurses.

## Overview

This library provides a component-based architecture for building ncurses applications with:
- Base Widget abstraction
- Window management
- Container/layout system
- Event handling
- Theme support

## Design Principles

- **Component-based**: Each UI element is a self-contained component
- **Composable**: Widgets can be nested within containers
- **Pluggable**: Easy to integrate into any ncurses application
- **Minimal dependencies**: Only depends on ncurses
- **Modern C++**: Uses C++20 features

## Architecture

```
Widget (abstract base)
  ├── Window (managed ncurses WINDOW*)
  ├── Container (holds child widgets)
  │   ├── VBox (vertical layout)
  │   ├── HBox (horizontal layout)
  │   └── Grid (grid layout)
  ├── TextBox (text input)
  ├── Label (static text)
  ├── Button (clickable button)
  ├── List (scrollable list)
  └── Panel (bordered container)
```

## Integration

Add to your CMakeLists.txt:

```cmake
add_subdirectory(lib/ui)
target_link_libraries(your_target ncurses_ui)
```

Include in your code:

```cpp
#include <ui/Widget.h>
#include <ui/Window.h>
#include <ui/Container.h>
```

## Building

The library is built as part of the parent project's build system.

```bash
mkdir build && cd build
cmake ..
make ncurses_ui
```

## License

MIT

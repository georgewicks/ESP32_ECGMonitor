# How to Make (and Edit) CMakeLists.txt in an ESP-IDF Project

A practical, project-specific tutorial for anyone contributing to **ESP32-ECGMonitor**.
It covers the ESP-IDF build system, the Espressif VS Code extension's editor support,
and a worked example of adding a new component — using the files already in this repo.

---

## 1. The big picture: three levels of CMakeLists.txt

An ESP-IDF project is built by **CMake**, but you don't write raw CMake — you mostly
fill in a small declarative file per *component*. There are three kinds:

| File | Who owns it | What goes in it |
|---|---|---|
| [`/CMakeLists.txt`](../CMakeLists.txt) | the project root | boilerplate: `project()`, `include(project.cmake)`, optional build settings |
| [`main/CMakeLists.txt`](../main/CMakeLists.txt) | the `main` component | sources + dependencies for the application entry point |
| [`components/<Name>/CMakeLists.txt`](../components) | each reusable component | sources + dependencies for that one component |

Our root `CMakeLists.txt` looks like this (it never changes much):

```cmake
cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)   # build only components we actually use
project(ESP32-ECGMonitor)
```

Everything else is expressed with **one function**: `idf_component_register()`.

---

## 2. `idf_component_register()` — the one function you need

This is the whole game. It declares a component's sources, include paths, and
dependencies. The keywords that matter day-to-day:

| Keyword | Meaning |
|---|---|
| `SRCS` | the `.c` files to compile (paths relative to this `CMakeLists.txt`) |
| `INCLUDE_DIRS` | directories whose headers are **public** — visible to this component *and* any component that `REQUIRES` it |
| `PRIV_INCLUDE_DIRS` | directories whose headers are **private** — visible only inside this component |
| `REQUIRES` | **public** dependencies: built first, and their public headers are visible to us *and* to anyone who requires us |
| `PRIV_REQUIRES` | **private** dependencies: built first, but their headers are visible only inside this component |

Real examples from this repo:

```cmake
# components/Logging/CMakeLists.txt — a tiny, dependency-light component
idf_component_register(SRCS "Logging.c"
                       INCLUDE_DIRS "."
                       REQUIRES "log")            # uses esp_log_set_vprintf()

# components/ECG-ISHNE/CMakeLists.txt — pure C, no ESP dependencies at all
idf_component_register(SRCS "ISHNE.c"
                       INCLUDE_DIRS ".")
```

```cmake
# main/CMakeLists.txt — the application component pulls everything together
idf_component_register(SRCS "ECG_Testing.c" "main.c" "I2C_Handling.c" "ECG_WiFi.c"
                       INCLUDE_DIRS "include" "."
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES "spi_flash" "esp_wifi" "nvs_flash" "esp_event"
                                "esp_timer" "driver" "log" "fatfs" "ads111x"
                                "ds3231" "PCF8574" "QRSDetect" "i2c_eeprom"
                                "Logging" "ECG-ISHNE"
                       PRIV_REQUIRES "spi_flash" "sdmmc")
```

### `REQUIRES` vs `PRIV_REQUIRES` — when to use which

- Your component's **public header** (in `INCLUDE_DIRS`) `#include`s another
  component's header → that other component must be in `REQUIRES` (or `PRIV_REQUIRES`).
- You only use the other component **inside your `.c` files**, and nobody else needs
  to see that dependency through you → use `PRIV_REQUIRES`.

> In practice: default to `REQUIRES` when in doubt. It builds the dependency and
> makes its headers available; the only "cost" is that dependents of *your* component
> also see it. `PRIV_REQUIRES` is an optimization for isolation, not a first resort.

---

## 3. Worked example: add a new component (5 minutes)

Let's add a hypothetical `components/Beeper` that `main` will call. We literally did
this for [`components/Logging`](../components/Logging) — follow the same steps.

**Step 1 — create the folder and files**

```
components/Beeper/
├── Beeper.h
├── Beeper.c
└── CMakeLists.txt
```

**Step 2 — write the component's `CMakeLists.txt`**

```cmake
idf_component_register(SRCS "Beeper.c"
                       INCLUDE_DIRS "."
                       REQUIRES "driver")   # Beeper.c uses driver/gpio.h
```

**Step 3 — tell `main` it needs the component**

Edit [`main/CMakeLists.txt`](../main/CMakeLists.txt) and add `"Beeper"` to `REQUIRES`.

**Step 4 — include the header in your code**

```c
#include "Beeper.h"
```

That's it. On the next build, CMake re-runs automatically (the build system detects
the changed `CMakeLists.txt`), compiles `Beeper.c`, and links it into the app.

> **Gotcha:** if a folder under `components/` has **no** `CMakeLists.txt`, ESP-IDF
> silently **skips it** and prints
> `-- Component directory ... does not contain a CMakeLists.txt file. No component will be added`.
> That's exactly what happened to `ECG-ISHNE` before it had one. If your component
> "isn't building," check for this message first.

---

## 4. The Espressif VS Code extension's CMake support

You don't have to write these files blind. The **Espressif IDF extension for VS Code**
provides editor assistance and project commands:

- **Autocomplete / IntelliSense** — open any `CMakeLists.txt` and the extension
  completes `idf_component_register` keywords, `project()`, `idf_build_set_property`,
  and component names as you type.
- **"ESP-IDF: Add Component"** — from the command palette it scaffolds a new
  `components/<Name>` directory with a template `CMakeLists.txt` and a starter `.c`/`.h`,
  so you don't have to create the files by hand.
- **Build / Clean / Full Clean** — the status-bar tools and the `ESP-IDF: Build project`,
  `ESP-IDF: Full Clean` commands run the CMake configure + Ninja build for you. Use
  **Full Clean** if a `CMakeLists.txt` change is ever not picked up (rare, but it cures
  stale-config weirdness).
- **Where files are edited** — the editor edits the `CMakeLists.txt` text directly;
  there is no separate "visual form." The "editor" *is* the text editor plus the
  extension's completion and diagnostics.

Quick sanity checklist after editing a `CMakeLists.txt`:

1. No `CMakeLists.txt` missing in a component you added.
2. Component name in `REQUIRES`/`PRIV_REQUIRES` matches the folder name **exactly**
   (`Logging` ≠ `logging`).
3. New `.c` files are listed in `SRCS`.
4. Headers you `#include` from other components live in an `INCLUDE_DIRS` of the
   component that owns them, and that component is in your `REQUIRES`.

---

## 5. Common errors and what they mean

| Symptom | Cause | Fix |
|---|---|---|
| `fatal error: X.h: No such file or directory` | header not in an `INCLUDE_DIRS`, or the owning component isn't in your `REQUIRES` | add the include dir and/or the dependency |
| `Component directory ... does not contain a CMakeLists.txt` | folder has no `CMakeLists.txt` | add one |
| `undefined reference to ...` | a source file isn't in `SRCS`, or a component isn't linked | add the `.c` to `SRCS` or the component to `REQUIRES` |
| `Could NOT find component 'Foo'` | `REQUIRES "Foo"` but no `components/Foo` exists (or name typo) | create it or fix the spelling |
| "Circular dependency" | A requires B and B requires A | break the cycle (move shared code to a lower-level component) |

---

## 6. Where to look for more

- **ESP-IDF Build System docs**:
  <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html>
- **`idf_component_register` reference**:
  <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-registration>
- **This repo's real components**: [`components/Logging`](../components/Logging),
  [`components/ECG-ISHNE`](../components/ECG-ISHNE), [`components/QRSDetect`](../components/QRSDetect) —
  three good templates ranging from "no dependencies" to "requires main."

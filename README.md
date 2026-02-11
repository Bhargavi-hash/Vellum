# Vellum

Extremely lightweight vector note-taking for Linux (Qt6). 
Vellum features smooth ink rendering, infinite and A4-notebook canvas modes, automatic geometric shape recognition, and SQLite-based persistence.

## Features
* **Vector-based Ink:** Pressure-sensitive strokes with smooth Join/Cap styles.
* **Shape Recognition:** Automatically transforms hand-drawn strokes into perfect lines, circles, and rectangles.
* **Smart Storage:** Fast, transactional saving using a localized SQLite database.
* **PDF Export:** High-fidelity export to A4-paginated PDF documents.

## Installation & Build

### 1. Install Dependencies
You will need a C++17 compatible compiler, CMake, and the Qt6 development libraries.

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y \
    cmake \
    g++ \
    qt6-base-dev \
    qt6-base-dev-tools \
    libqt6sql6-sqlite \
    qt6-pdf-dev \
    qt6-printsupport-dev
```

### 2. Build the Project
Using CMake, generate the build files and compile the executable:
```bash
# Configure the build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Compile using all available cores
cmake --build build -j$(nproc)
```

### 3. Run Vellum
```bash
./build/vellum
```

## Project Structure

- `src/model/:` Core data structures (Strokes, TextBoxes) and the Command pattern logic.
- `src/canvas/:` The custom Qt6 Widget for low-latency ink rendering.
- `src/storage/:` SQLite backend for document persistence.
- `src/shapes/:` Heuristic-based geometric shape recognizer.
- `src/export/:` PDF generation logic using QPdfWriter.

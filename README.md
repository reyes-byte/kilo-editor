# Kilo: A Dependency-Free Terminal Text Editor

A lightweight, fully functional terminal text editor written entirely in standard C from scratch. This project bypasses standard libraries like `ncurses` and directly interacts with the terminal using raw POSIX standard input/output and VT100 escape sequences.

## ⚡ Core Features

* **Zero Dependencies:** Built entirely with standard C libraries (`libc`).
* **Raw Terminal I/O:** Manually processes byte-level input and VT100 escape sequences for screen clearing, cursor positioning, and text formatting.
* **Custom Lexical Analyzer:** A hand-rolled syntax highlighting engine built specifically for C code.
* **Dynamic Memory Management:** Seamlessly handles row insertions, deletions, and scrolling through dynamic memory reallocation.
* **Search Functionality:** Incremental forward and backward string search across the entire file buffer.

## 🧠 Under the Hood: The Syntax Engine

The crown jewel of this editor is the custom state-machine used for syntax highlighting. Instead of relying on regular expressions, it parses characters sequentially, handling several complex edge cases:

* **String Literal Shielding:** Prevents comments from triggering inside `""` or `''`.
* **Keyword Detection:** Uses `strncmp` and custom separator checks to isolate primary and secondary C keywords.
* **Cross-Row State Persistence (The Ripple Effect):** Multi-line comments (`/* ... */`) utilize a cross-row boolean flag (`hl_open_comment`). Upward edits trigger a highly efficient recursive cascade that passes the state down the file, utilizing a "kill-switch" to halt the recursion the moment the state synchronizes, saving vital CPU cycles.

## 🛠️ Building and Running

Because Kilo has no external dependencies, compiling it is incredibly fast and simple.

### Prerequisites
* A C compiler (GCC or Clang)
* `make`

### Installation
```bash
git clone [https://github.com/YOUR_USERNAME/kilo-editor.git](https://github.com/YOUR_USERNAME/kilo-editor.git)
cd kilo-editor
make

## 📜 Acknowledgements

This project was built following the [Build Your Own Text Editor](https://viewsourcecode.org/snaptoken/kilo/) tutorial by Jeremy Ruten, which is based on `kilo` by [antirez](https://github.com/antirez/kilo).

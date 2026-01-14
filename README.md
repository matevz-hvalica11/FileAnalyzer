# FileAnalyzer (C++)

High-performance multithreaded file system analyzer written in modern C++.


## Features
  - Recursive directory scanning using `std::filesystem`
  - Multithreaded processing with worker threads
  - Thread-safe aggregation (mutex + condition_variable)
  - Reports:
    - Total size
    - Largest files
    - Most common file types
  - Generates a detailed report file


## Example Output

Total size (GB): 8.84
Top 50 largest files:
0.28 GB - C:\Windows\System32\Microsoft-Edge-WebView\msedge.dll


## Build
Requires a C++17-compatible compiler.

Example (MSVC):

cl /std:c++17 FileAnalyzer.cpp


## Usage

FileAnalyzer <directory_path>

Example:

FileAnalyzer C:\Windows\System32


## Notes
This project is designed to handle large real-world directories and demonstrate practical multithreading and synchronization in C++.

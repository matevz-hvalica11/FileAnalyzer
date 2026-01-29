# FileAnalyzer (C++)


<img width="1483" height="762" alt="Posnetek zaslona 2026-01-29 070840" src="https://github.com/user-attachments/assets/45e21f2f-9633-40e7-a3ec-dfb3a272e4f3" />

A high-performance, multithreaded file-system analyzer written in modern C++17.
Built to handle massive directories efficiently while providing clean, structured output.

## Features
🔍 Recursive directory scanning via std::filesystem

⚡ Multithreaded processing using a worker-thread queue

🔐 Thread-safe aggregation with mutexes + condition variables

📊 Reports include:

  - Total file count

  - Total size (in GB)

  - Largest files

  - File extensions ranked by total size

  - Percentage breakdowns


📝 Automatically generates a detailed report file


## Example Output

FileAnalyzer starting...
Scanning path: C:\Windows\System32

Total files: 42762  
Total size: 8.84 GB  

Top 50 largest files:
0.28 GB - C:\Windows\System32\Microsoft-Edge-WebView\msedge.dll
...


## Build instructions
Requires C++17 or newer.

Example (MSVC):

cl /std:c++17 FileAnalyzer.cpp


## Linux / clang / g++ Example
g++ -std=c++17 -O2 FileAnalyzer.cpp -o FileAnalyzer -lpthread


## Usage

FileAnalyzer <directory_path>

Example:

FileAnalyzer C:\Windows\System32


## Notes
This project was built to handle large real-world directory trees, stress-test multithreading, and serve as a practical example of synchronization, queue-based workload distribution, and modern C++ filesystem APIs.

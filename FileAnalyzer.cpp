// FileAnalyzer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <filesystem>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <fstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace fs = std::filesystem;

void processFile(
    const fs::directory_entry& entry,
    std::size_t& count,
    std::uintmax_t& totalSize,
    std::vector<std::pair<std::uintmax_t, fs::path>>& allFiles,
    std::unordered_map<std::string, std::size_t>& fileTypes,
    std::mutex& dataMutex)
{
    if (!entry.is_regular_file())
        return;

    auto size = entry.file_size();
    std::string ext = entry.path().extension().string();

    if (ext.empty())
        ext = "(no ext)";
    else
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);


    // Lock before touching shared data
    std::lock_guard<std::mutex> lock(dataMutex);

    ++count;
    totalSize += size;
    allFiles.emplace_back(size, entry.path());
    ++fileTypes[ext];
}


// Worker function
void workerThread(
    std::queue<fs::directory_entry>& workQueue,
    std::mutex& queueMutex,
    std::condition_variable& cv,
    bool& doneScanning,
    std::size_t& count,
    std::uintmax_t& totalSize,
    std::vector<std::pair<std::uintmax_t, fs::path>>& allFiles,
    std::unordered_map<std::string, std::size_t>& fileTypes,
    std::mutex& dataMutex)
{
    while (true)
    {
        fs::directory_entry entry;

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [&]()
                {
                    return !workQueue.empty() || doneScanning;
                });

            if (workQueue.empty() && doneScanning)
                return;

            entry = workQueue.front();
            workQueue.pop();
        }

        if (!entry.is_regular_file())
            continue;

        auto size = entry.file_size();
        std::string ext = entry.path().extension().string();

        if (ext.empty())
            ext = "(no ext)";
        else
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        {
            std::lock_guard<std::mutex> lock(dataMutex);
            ++count;
            totalSize += size;
            allFiles.emplace_back(size, entry.path());
            ++fileTypes[ext];
        }
    }
}

int main(int argc, char* argv[])
{

    // Variable declarations
    std::size_t count = 0;
    std::uintmax_t totalSize = 0;
    std::vector<std::pair<std::uintmax_t, fs::path>> allFiles;
    std::unordered_map<std::string, std::size_t> fileTypes;
    std::mutex dataMutex;
    std::vector<std::thread> workers;
    std::queue<fs::directory_entry> workQueue;
    std::mutex queueMutex;
    std::condition_variable cv;

    bool doneScanning = false;

    const unsigned int maxThreads = std::thread::hardware_concurrency();

    std::cout << "FileAnalyzer starting...\n";

    if (argc < 2)
    {
        std::cout << "Usage: FileAnalyzer <directory_path>\n";
        return 1;
    }

    fs::path rootPath = argv[1];
    std::cout << "Scanning path: [" << rootPath.string() << "]\n";


    // Start worker threads
    const unsigned int threadCount = std::thread::hardware_concurrency();

    for (unsigned int i = 0; i < threadCount; ++i)
    {
        workers.emplace_back(
            workerThread,
            std::ref(workQueue),
            std::ref(queueMutex),
            std::ref(cv),
            std::ref(doneScanning),
            std::ref(count),
            std::ref(totalSize),
            std::ref(allFiles),
            std::ref(fileTypes),
            std::ref(dataMutex));
    }

    if (!fs::exists(rootPath))
    {
        std::cout << "Path does not exist\n";
        return 1;
    }

    if (!fs::is_directory(rootPath))
    {
        std::cout << "Path is not a directory\n";
        return 1;
    }

    try
    {
        // Scan loop
        for (const auto& entry : fs::recursive_directory_iterator(
            rootPath,
            fs::directory_options::skip_permission_denied |
            fs::directory_options::follow_directory_symlink))
        {
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                workQueue.push(entry);
            }
            cv.notify_one();
        }

        // Shutdown workers cleanly
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            doneScanning = true;
        }
        cv.notify_all();

        for (auto& t : workers)
            t.join();

        // Sort everything
        std::sort(allFiles.begin(), allFiles.end(),
            [](const auto& a, const auto& b)
            {
                return a.first > b.first;
            });

        std::ofstream report("report.txt");
        if (!report)
        {
            std::cerr << "Failed to create report.txt\n";
            return 1;
        }

        auto now = std::chrono::system_clock::now();
        std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

        char timeBuffer[26];
        ctime_s(timeBuffer, sizeof(timeBuffer), &nowTime);

        report << "FileAnalyzer Report\n";
        report << "Scanned path: " << rootPath.string() << '\n';
        report << "Generated at: " << timeBuffer;
        report << "----------------------------------------\n\n";


        // Convert size after loop
        double totalSizeGB = totalSize / (1024.0 * 1024 * 1024);


        // Report file
        report << "\nTop 50 largest files:\n";
        for (std::size_t i = 0; i < 50 && i < allFiles.size(); ++i)
        {
            double sizeGB = allFiles[i].first / (1024.0 * 1024 * 1024);
            report << std::fixed << std::setprecision(2)
                << sizeGB << " GB - "
                << allFiles[i].second.string() << '\n';
        }

        std::vector<std::pair<std::string, std::size_t>> sortedTypes(
            fileTypes.begin(), fileTypes.end());

        std::sort(sortedTypes.begin(), sortedTypes.end(),
            [](const auto& a, const auto& b)
            {
                return a.second > b.second;
            });

        report << "\nTop 10 file types:\n";
        for (std::size_t i = 0; i < 10 && i < sortedTypes.size(); ++i)
        {
            report << sortedTypes[i].first
                << " : "
                << sortedTypes[i].second
                << '\n';
        }

        report.close();


        // Console output
        std::cout << "Total size (GB): "
            << std::fixed << std::setprecision(2)
            << totalSizeGB << '\n';

        std::cout << "\nTop 50 largest files:\n";
        for (std::size_t i = 0; i < 50 && i < allFiles.size(); ++i)
        {
            double sizeGB = allFiles[i].first / (1024.0 * 1024 * 1024);
            std::cout << sizeGB << " GB - " << allFiles[i].second.string() << '\n';
        }

        std::cout << "\nTop 10 file types:\n";
        for (std::size_t i = 0; i < 10 && i < sortedTypes.size(); ++i)
        {
            std::cout << sortedTypes[i].first
                << " : "
                << sortedTypes[i].second
                << '\n';
        }
    }
    catch (const fs::filesystem_error& e)
    {
        std::cerr << "Filesystem error: " << e.what() << '\n';
    }

    return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

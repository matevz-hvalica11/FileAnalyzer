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


// Simple ANSI Colors (work everywhere)
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RED "\033[31m"
#define CYAN "\033[36m"
#define RESET "\033[0m"


// Global Shared State
std::size_t g_count = 0;
std::uintmax_t g_totalSize = 0;

std::unordered_map<std::string, std::uintmax_t> g_extTotalSize;
std::mutex g_dataMutex;

std::queue<fs::directory_entry> g_workQueue;
std::mutex g_queueMatex;
std::condition_variable g_cv;

bool g_doneScanning = false;


// Worker Thread Function
void worker()
{
	while (true)
	{
		fs::directory_entry entry;

		{
			// Wait for work
			std::unique_lock<std::mutex> lock(g_queueMatex);
			g_cv.wait(lock, []()
			{
				return !g_workQueue.empty() || g_doneScanning;
			});

			if (g_workQueue.empty() && g_doneScanning)
				return;

			entry = g_workQueue.front();
			g_workQueue.pop();
		}

		// Process file
		if (!entry.is_regular_file())
			continue;

		auto size = entry.file_size();
		std::string ext = entry.path().extension().string();

		if (ext.empty())
			ext = "(no ext)";
		else
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		std::lock_guard<std::mutex> lock(g_dataMutex);

		g_count++;
		g_totalSize += size;
		g_extTotalSize[ext] += size;
	}
}


// Main

int main(int argc, char* argv[])
{
	std::cout << CYAN "FileAnalyzer starting...\n" << RESET;

	if (argc < 2)
	{
		std::cout << RED << "Usage: FileAnalyzer <directory_path>\n" << RESET;
		return 1;
	}

	fs::path rootPath = argv[1];
	std::cout << "Scanning path: " << rootPath.string() << "\n";

	if (!fs::exists(rootPath) || !fs::is_directory(rootPath))
	{
		std::cout << RED << "Invalid path.\n" << RESET;
		return 1;
	}

	// Launch worker threads
	unsigned int threadCount = std::max(2u, std::thread::hardware_concurrency());
	std::vector<std::thread> threads;

	for (unsigned int i = 0; i < threadCount; i++)
		threads.emplace_back(worker);

	// Feed Queue
	try
	{
		for (const auto& entry : fs::recursive_directory_iterator(
			rootPath,
			fs::directory_options::skip_permission_denied |
			fs::directory_options::follow_directory_symlink))
		{
			{
				std::lock_guard<std::mutex> lock(g_queueMatex);
				g_workQueue.push(entry);
			}
			g_cv.notify_one();
		}
	}
	catch (const fs::filesystem_error& e)
	{
		std::cerr << RED << "Filesystem error: " << e.what() << RESET << '\n';
	}

	// Signal shutdown
	{
		std::lock_guard<std::mutex> lock(g_queueMatex);
		g_doneScanning = true;
	}
	g_cv.notify_all();

	// Wait for workers
	for (auto& t : threads)
		t.join();

	// Reporting + sorting
	double totalGB = g_totalSize / (1024.0 * 1024 * 1024);

	// Sort extensions by total size
	std::vector<std::pair<std::string, std::uintmax_t>> sortedExts(
		g_extTotalSize.begin(), g_extTotalSize.end()
	);
	
	std::sort(sortedExts.begin(), sortedExts.end(),
		[](auto& a, auto& b)
		{
			return a.second > b.second;
		});


	// Output Results
	std::cout << GREEN << "\nScan finished.\n" << RESET;
	std::cout << "Total files: " << g_count << "\n";
	std::cout << "Total size: " << std::fixed << std::setprecision(2)
		<< totalGB << " GB\n\n";

	std::cout << YELLOW << "Top 15 file extensions by TOTAL SIZE:\n" << RESET;

	for (size_t i = 0; i < 15 && i < sortedExts.size(); i++)
	{
		double gb = sortedExts[i].second / (1024.0 * 1024 * 1024);
		double pct = (sortedExts[i].second / (double)g_totalSize) * 100.0;

		std::cout << CYAN
			<< std::setw(8) << sortedExts[i].first << RESET
			<< " : "
			<< std::setw(8) << std::fixed << std::setprecision(2) << gb
			<< " GB  (" << std::setprecision(1) << pct << "%\n";
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

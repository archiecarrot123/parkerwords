#include <bit>
#include <ctime>
#include <vector>
#include <deque>
#include <string>
#include <unordered_map>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <functional>
#include <algorithm>
#include <array>
#include <chrono>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>

constexpr int MaxThreads = 16;

// uncomment this line to write info to stdout, which takes away precious CPU time
#define NO_OUTPUT


#ifdef NO_OUTPUT
#define OUTPUT(x) do{;}while(false)
#else
#define OUTPUT(x) do{x;}while(false)
#endif


using uint = unsigned int;

std::vector<uint> wordbits;
std::vector<std::string> allwords;
std::unordered_map<uint, size_t> bitstoindex;
std::vector<uint> letterindex[26];
uint letterorder[26];

std::string_view getword(const char*& _str, const char* end)
{
	const char* str = _str;
	while(*str == '\n' || *str == '\r')
	{
		if (++str == end)
			return (_str = str), std::string_view{};
	}

	const char* start = str;
	while(str != end && *str != '\n' && *str != '\r')
		++str;

	_str = str;
	return std::string_view{ start, str };
}

uint getbits(std::string_view word)
{
	uint r = 0;
	for (char c : word)
		r |= 1 << (c - 'a');
	return r;
}

void readwords(const char* file)
{
	struct { int f, l; } freq[26] = { };
	for (int i = 0; i < 26; i++)
		freq[i].l = i;

	// open file
	std::vector<char> buf;
	std::ifstream in(file);
	in.seekg(0, std::ios::end);
	buf.resize(in.tellg());
	in.seekg(0, std::ios::beg);
	in.read(&buf[0], buf.size());

	const char* str = &buf[0];
	const char* strEnd = str + buf.size();

	// read words
	std::string_view word;
	while(!(word = getword(str, strEnd)).empty())
	{
		if (word.size() != 5)
			continue;
		uint bits = getbits(word);
		if (std::popcount(bits) != 5)
			continue;

		if (!bitstoindex.contains(bits))
		{
			bitstoindex[bits] = wordbits.size();
			wordbits.push_back(bits);
			allwords.emplace_back(word);

			// count letter frequency
			for(char c: word)
				freq[c - 'a'].f++;
		}
	}

	// rearrange letter order based on lettter frequency (least used letter gets lowest index)
	std::sort(std::begin(freq), std::end(freq), [](auto a, auto b) { return a.f < b.f; });
	uint reverseletterorder[26];
	for (int i = 0; i < 26; i++)
	{
		letterorder[i] = freq[i].l;
		reverseletterorder[freq[i].l] = i;
	}

	// build index based on least used letter
	for (uint w : wordbits)
	{
		uint m = w;
		uint letter = std::countr_zero(m);
		uint min = reverseletterorder[letter];
		m &= m - 1; // drop lowest set bit
		while(m)
		{
			letter = std::countr_zero(m);
			min = std::min(min, reverseletterorder[letter]);
			m &= m - 1;
		}

		letterindex[min].push_back(w);
	}
}

using WordArray = std::array<uint, 5>;
using OutputFn = std::function<void(const WordArray&)>;

long long start;
long long timeUS() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count(); }


struct State
{
	uint totalbits;
	int numwords;
	WordArray words;
	uint maxletter;
	bool skipped;
	bool stop;
};
std::mutex queueMutex;
std::condition_variable queueCVar;
std::deque<State> queue;

void findwords(std::vector<WordArray>& solutions, uint totalbits, int numwords, WordArray words, uint maxLetter, bool skipped, bool force = false)
{
	if (numwords == 5)
	{
		solutions.push_back(words);
		return;
	}

	if (!force && numwords == 1)
	{
		{
			std::unique_lock lock{ queueMutex };
			queue.push_back({ .totalbits = totalbits, .numwords = numwords, .words = words, .maxletter = maxLetter, .skipped = skipped, .stop = false });
		}
		queueCVar.notify_one();
		return;
	}

// 	size_t max = wordbits.size();

	// walk over all letters in a certain order until we find an unused one
	for (uint i = maxLetter; i < 26; i++)
	{
		uint letter = letterorder[i];
		uint m = 1 << letter;
		if (totalbits & m)
			continue;

		// take all words from the index of this letter and add each word to the solution if all letters of the word aren't used before.
		for (uint w : letterindex[i])
		{
			if (totalbits & w)
				continue;

			words[numwords] = w;
			findwords(solutions, totalbits | w, numwords + 1, words, i + 1, skipped);

			OUTPUT(if (numwords == 0) std::cout << "\33[2K\rFound " << numsolutions << " solutions. Running time: " << (timeUS() - start) << "us");
		}

		if (skipped)
			break;
		skipped = true;
	}
}

void findthread(std::vector<WordArray>& solutions)
{
	std::vector<WordArray> mysolutions;

	std::unique_lock lock{ queueMutex };
	for(;;)
	{
		if (queue.empty())
			queueCVar.wait(lock, []{ return !queue.empty(); });
		State state = queue.front();
		queue.pop_front();
		if (state.stop)
			break;
		lock.unlock();
		findwords(mysolutions, state.totalbits, state.numwords, state.words, state.maxletter, state.skipped, true);
		lock.lock();
	}

	solutions.insert(solutions.end(), mysolutions.begin(), mysolutions.end());
}


int findwords(std::vector<WordArray>& solutions)
{
	std::vector<std::jthread> threads;
	auto numThreads = std::thread::hardware_concurrency() - 1;
	threads.reserve(numThreads);

	for (uint i = 0; i < numThreads; i++)
		threads.emplace_back([&]() { findthread(solutions); });

	WordArray words = { };
	findwords(solutions, 0, 0, words, 0, false);

	{
		std::unique_lock lock{ queueMutex };
		for (uint i = 0; i < numThreads; i++)
			queue.push_back({ .stop = true });
		queueCVar.notify_all();
	}
	threads.clear();

	return int(solutions.size());
}

int main()
{
	start = timeUS();
	readwords("words_alpha.txt");
	std::vector<WordArray> solutions;
	solutions.reserve(10000);

	OUTPUT(
		std::cout << wordbits.size() << " unique words\n";
		std::cout << "letter order: ";
		for (int i = 0; i < 26; i++)
			std::cout << char('a' + letterorder[i]);
		std::cout << "\n";
	);

	auto startAlgo = timeUS();
	int num = findwords(solutions);

	auto startOutput = timeUS();
	std::ofstream out("solutions.txt");
	for (auto& words : solutions)
	{
		for (auto w : words)
			out << allwords[bitstoindex[w]] << "\t";
		out << "\n";
	};

	OUTPUT(std::cout << "\n");

	long long end = timeUS();
	std::cout << num << " solutions written to solutions.txt.\n";
	std::cout << "Total time: " << end - start << "us (" << (end - start) / 1.e6f << "s)\n";
	std::cout << "Read:		" << std::setw(8) << startAlgo - start << "us\n";
	std::cout << "Process:	" << std::setw(8) << startOutput - startAlgo << "us\n";
	std::cout << "Write:	" << std::setw(8) << end - startOutput << "us\n";
}

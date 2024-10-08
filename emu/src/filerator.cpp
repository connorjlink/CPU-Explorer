#include "filerator.hpp"

#include <fstream>

//loads the contents of a specified file into a string
std::vector<std::uint8_t> cpu::filerator(const char* path) noexcept
{
	//open the stream to load the file
	std::basic_ifstream<std::uint8_t> f(path, std::ios::binary);

	//retrieve the file size
	f.seekg(0, std::ios::end);
	auto size = f.tellg();
	f.seekg(0, std::ios::end);

	//reserve memory buffer on the heap
	std::vector<std::uint8_t> results(size);

	//read the file into the buffer
	f.read(results.data(), size);

	//return the newly created buffer
	return results;
}
#include "assert.hpp"

#include <iostream>
#include <cstdlib>

//function that performs a debug assertion exit
void ::_assert(const char* message, const char* function, const char* filename, unsigned line)
{
	std::cerr << "\a[" << function << "] Debug assertion failed! Faulting expression : " << message << '\n';
	std::cerr << "File: " << filename << " on line " << line << '\n';
	std::exit(line);
}
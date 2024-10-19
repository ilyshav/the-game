#include <fstream>
#include <iostream>
#include <vector>

static std::vector<char> readFile(const std::string &filename)
{
	std::cout << std::string("Loading file ") + filename + "\n";
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("failed to open file!");
	}

	size_t            fileSize = (size_t) file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();
	std::cout << std::string("Loading of ") + filename + " is done\n";

	return buffer;
}
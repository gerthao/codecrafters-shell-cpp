#include <iostream>
#include <string>
#include <print>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::print("$ ");

  auto input = std::string{};
  std::getline(std::cin, input);

  std::println("{}: command not found", input);
}

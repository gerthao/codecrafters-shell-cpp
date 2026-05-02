#include <iostream>
#include <string>
#include <print>

[[noreturn]] int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  while (true) {
    std::print("$ ");
    std::string input;
    std::getline(std::cin, input);
    std::println("{}: command not found", input);
  }
}

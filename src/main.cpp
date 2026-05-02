#include <iostream>
#include <string>
#include <print>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true) {
    std::print("$ ");
    std::string input;
    std::getline(std::cin, input);

    if (input == "exit") {
      return 0;
    }

    std::println("{}: command not found", input);
  }
}

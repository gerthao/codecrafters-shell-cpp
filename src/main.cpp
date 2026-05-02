#include <iostream>
#include <string>
#include <print>
#include <ranges>
#include <vector>
#include <istream>
#include <sstream>

void echo(std::ranges::input_range auto input) {
    for (auto i = 0; i < input.size() - 1; ++i) {
        std::print("{} ", input[i]);
    }
    std::print("{}", input.back());

    std::println( "");
}

std::vector<std::string> parse_and_trim_input(const std::string &input) {
    std::istringstream stream(input);
    std::vector<std::string> tokens{
        std::istream_iterator<std::string>{stream},
        std::istream_iterator<std::string>{}
    };
    return tokens;
}

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

        auto tokens = parse_and_trim_input(input);
        if (tokens.empty()) {
            continue;
        }

        auto head = tokens.front();
        auto tail = tokens | std::views::drop(1);

        if (head == "echo") {
            echo(tail);
        } else {
            std::println("{}: command not found", input);
        }
    }
}

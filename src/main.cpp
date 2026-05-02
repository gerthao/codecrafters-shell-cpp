#include <iostream>
#include <string>
#include <print>
#include <ranges>
#include <vector>
#include <istream>
#include <sstream>

enum class Command { Echo, Type, Exit };

std::optional<Command> parse_command(const std::string& input) {
    if (input == "echo")
        return Command::Echo;
    if (input == "type")
        return Command::Type;
    if (input == "exit")
        return Command::Exit;
    return std::nullopt;
}

bool is_builtin(const std::string& command) {
    return command == "echo" || command == "type" || command == "exit";
}

void run_type(std::ranges::input_range auto input) {
    for (auto token: input) {
        auto maybe_command = parse_command(token);

        if (maybe_command.has_value()) {
            std::println("{} is a shell builtin", token);
        } else {
            std::println("{}: not found", token);
        }
    }
}

void run_echo(std::ranges::input_range auto input) {
    for (auto i = 0; i < input.size() - 1; ++i) {
        std::print("{} ", input[i]);
    }
    std::print("{}", input.back());

    std::println( "");
}

void run_command(const Command& command, std::ranges::input_range auto args) {
    if (command == Command::Type) {
        run_type(args);
    } else if (command == Command::Echo) {
        run_echo(args);
    }
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

        auto maybe_command = parse_command(tokens.front());
        auto tail = tokens | std::views::drop(1);

        if (!maybe_command.has_value()) {
            std::println("{}: command not found", input);
            continue;
        }

        auto command = maybe_command.value();

        if (command == Command::Exit) {
            return 0;
        }

        run_command(command, tail);
    }
}

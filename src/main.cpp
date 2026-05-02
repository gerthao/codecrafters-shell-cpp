#include <iostream>
#include <string>
#include <print>
#include <ranges>
#include <vector>
#include <istream>
#include <sstream>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>

enum class Command { Cd, Echo, Exit, Pwd, Type };

std::tuple<std::string, std::ranges::drop_view<std::ranges::ref_view<const std::vector<std::string>> > >
get_head_and_tail(const std::vector<std::string> &tokens) {
    return std::make_tuple(tokens.front(), tokens | std::views::drop(1));
}

std::optional<Command> parse_command(const std::string &input) {
    if (input == "cd")
        return Command::Cd;
    if (input == "echo")
        return Command::Echo;
    if (input == "exit")
        return Command::Exit;
    if (input == "pwd")
        return Command::Pwd;
    if (input == "type")
        return Command::Type;

    return std::nullopt;
}

bool file_exists(const std::string &path) {
    return std::filesystem::exists(path);
}

bool file_has_execute_permission(const std::string &path) {
    const auto permissions = std::filesystem::status(path).permissions();
    return (permissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none;
}

std::optional<std::string> find_command_in_path_env_var(const std::string &command) {
    const auto path_env_var = std::getenv("PATH");

    if (path_env_var == nullptr) {
        return std::nullopt;
    }

    for (const auto &path: std::views::split(std::string(path_env_var), ':')) {
        const auto path_str = std::ranges::to<std::string>(path);
        auto full_path = path_str;
        full_path.append("/");
        full_path.append(command);

        if (file_exists(full_path) && file_has_execute_permission(full_path)) {
            return full_path;
        }
    }

    return std::nullopt;
}

void run_type(std::ranges::input_range auto input) {
    for (const auto &token: input) {
        if (const auto maybe_command = parse_command(token); maybe_command.has_value()) {
            std::println("{} is a shell builtin", token);
        } else if (const auto maybe_path = find_command_in_path_env_var(token); maybe_path.has_value()) {
            std::println("{} is {}", token, maybe_path.value());
        } else {
            std::println("{}: not found", token);
        }
    }
}

void run_pwd() {
    std::println("{}", std::filesystem::current_path().string());
}

void run_echo(std::ranges::input_range auto input) {
    for (auto i = 0; i < input.size() - 1; ++i) {
        std::print("{} ", input[i]);
    }
    std::print("{}", input.back());

    std::println("");
}

void run_cd(const std::string &path) {
    if (!std::filesystem::exists(path))
        return;

    std::filesystem::current_path(path);
}

void run_command(const Command &command, std::ranges::input_range auto args) {
    switch (command) {
        case Command::Cd:
            run_cd(args.front());
            break;
        case Command::Echo:
            run_echo(args);
            break;
        case Command::Pwd:
            run_pwd();
            break;
        case Command::Type:
            run_type(args);
            break;
        default:
            break;
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

        auto tokens = parse_and_trim_input(input);

        if (tokens.empty()) {
            continue;
        }

        const auto [command_name, command_args] = get_head_and_tail(tokens);

        if (const auto maybe_command = parse_command(command_name); maybe_command.has_value()) {
            const auto command = maybe_command.value();

            if (command == Command::Exit) {
                return 0;
            }

            run_command(command, command_args);
            continue;
        }

        if (const auto maybe_program_path = find_command_in_path_env_var(command_name);
            maybe_program_path.has_value()) {
            const auto &program_path = maybe_program_path.value();

            const pid_t pid = fork();

            if (pid < 0) return 1;

            if (pid == 0) {
                std::vector<char *> args;

                args.push_back(const_cast<char *>(command_name.c_str()));
                for (const auto &token: command_args) {
                    args.push_back(const_cast<char *>(token.c_str()));
                }
                args.push_back(nullptr);

                if (execvp(program_path.c_str(), args.data()) == -1) {
                    _exit(1);
                }
            } else {
                int status;
                waitpid(pid, &status, 0);
            }

            continue;
        }

        std::println("{}: command not found", command_name);
    }
}

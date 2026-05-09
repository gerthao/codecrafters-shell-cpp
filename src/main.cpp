#include <iostream>
#include <string>
#include <print>
#include <ranges>
#include <vector>
#include <sstream>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>
#include <algorithm>
#include <fstream>
#include <fcntl.h>

constexpr char BACKSLASH = '\\';
constexpr char DOUBLE_QUOTE = '"';
constexpr char SINGLE_QUOTE = '\'';
constexpr char WHITESPACE = ' ';
constexpr char NEWLINE = '\n';
constexpr char REDIRECT_CHAR = '>';

enum class Command { Cd, Echo, Exit, Pwd, Type };

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

std::optional<std::string> find_command_in_path_env_var(const std::string& command) {
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

void run_type(std::ranges::input_range auto input, std::ostream& output_os = std::cout) {
    for (const auto &token: input) {
        if (const auto maybe_command = parse_command(token); maybe_command.has_value()) {
            std::println(output_os, "{} is a shell builtin", token);
        } else if (const auto maybe_path = find_command_in_path_env_var(token); maybe_path.has_value()) {
            std::println(output_os, "{} is {}", token, maybe_path.value());
        } else {
            std::println("{}: not found", token);
        }
    }
}

void run_pwd(std::ostream& output_os = std::cout) {
    std::println(output_os, "{}", std::filesystem::current_path().string());
}

void run_echo(std::ranges::range auto input, std::ostream& output_os = std::cout) {
    for (auto i = 0; i < input.size() - 1; ++i) {
        std::print(output_os, "{} ", input[i]);
    }
    std::print(output_os, "{}", input.back());
    std::println(output_os, "");
}

void run_cd(const std::string& path) {
    if (path == "~")
        std::filesystem::current_path(std::getenv("HOME"));
    else if (std::filesystem::exists(path))
        std::filesystem::current_path(path);
    else
        std::println("cd: {}: No such file or directory", path);
}

void run_external_program(const std::string& program_path, const std::vector<std::string>& command_args,
                          const std::string& file_name) {
    std::vector<char *> args;
    for (const auto &token: command_args) {
        args.push_back(const_cast<char *>(token.c_str()));
    }
    args.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        const int file_descriptor = open(file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (file_descriptor < 0) {
            _exit(1);
        }

        if (dup2(file_descriptor, STDOUT_FILENO) < 0) {
            _exit(1);
        }

        close(file_descriptor);

        if (execvp(program_path.c_str(), args.data()) == -1) {
            _exit(1);
        }
    } else {
        int status;
        waitpid(pid, &status, 0);
    }
}

std::vector<std::string> tokenize_input(const std::string &input) {
    std::vector<std::string> tokens;
    std::string current;
    std::string input_copy = input;

    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool is_using_backslash = false;
    auto in_quoting_mode = [&] { return in_single_quotes || in_double_quotes; };

    do {
        for (const auto &c: input_copy) {
            if (is_using_backslash && !in_single_quotes) {
                current += c;
                is_using_backslash = false;
            } else if (c == BACKSLASH && !in_single_quotes)
                is_using_backslash = true;
            else if (c == DOUBLE_QUOTE && !in_single_quotes)
                in_double_quotes = !in_double_quotes;
            else if (c == SINGLE_QUOTE && !in_double_quotes)
                in_single_quotes = !in_single_quotes;
            else if (c == WHITESPACE && !in_quoting_mode()) {
                if (current.empty()) continue;

                tokens.push_back(current);
                current.clear();
            } else
                current += c;
        }

        // handles last part of input after iteration ends
        if (!current.empty()) {
            // when user enters new-line
            if (in_quoting_mode()) {
                current += NEWLINE;
                std::getline(std::cin, input_copy);
                continue;
            }

            tokens.push_back(current);
            current.clear();
        }
    } while (in_quoting_mode());

    return tokens;
}

int main() {
    // Flush after every std::cout / std:cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    for (;;) {
        std::print("$ ");
        std::string input;
        std::getline(std::cin, input);

        auto tokens = tokenize_input(input);
        if (tokens.empty()) {
            continue;
        }

        const auto has_redirection = std::ranges::any_of(tokens, [](const auto &t) {
            return t == ">" || t == "1>";
        });

        const auto command_args = tokens | std::views::take_while([](const auto &t) {
            return t != ">" && t != "1>";
        });

        const auto command_args_vec = std::ranges::to<std::vector<std::string> >(command_args);
        const auto &command_name = command_args_vec.at(0);

        const auto file_name = has_redirection
                                          ? (tokens
                                             | std::views::drop_while([](const auto &t) {
                                                 return t != ">" && t != "1>";
                                             })
                                             | std::views::drop(1)).front()
                                          : "";

        std::ofstream file_stream;
        if (has_redirection && !file_name.empty()) {
            file_stream.open(file_name);
        }

        std::ostream& output_os = has_redirection ? file_stream : std::cout;

        if (const auto opt_command = parse_command(command_name); opt_command.has_value()) {
            switch (opt_command.value()) {
                case Command::Exit:
                    return 0;
                case Command::Echo:
                    run_echo(command_args_vec | std::views::drop(1), output_os);
                    break;
                case Command::Pwd:
                    run_pwd();
                    break;
                case Command::Type:
                    run_type(command_args_vec | std::views::drop(1), output_os);
                    break;
                case Command::Cd:
                    run_cd((command_args_vec | std::views::drop(1)).front());
                    break;
                default:
                    break;
            }
            continue;
        }

        if (const auto maybe_program_path = find_command_in_path_env_var(command_name); maybe_program_path.
            has_value()) {
            const auto& program_path = maybe_program_path.value();

            run_external_program(program_path, command_args_vec, file_name);
            continue;
        }

        std::println("{}: command not found", command_name);
    }
}

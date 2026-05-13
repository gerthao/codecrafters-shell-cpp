#include <iostream>
#include <string>
#include <print>
#include <ranges>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <ostream>
#include <optional>
#include <unistd.h>
#include <sys/wait.h>
#include <utility>
#include <fcntl.h>


template<class... Ts>
struct visitor : Ts... {
    using Ts::operator()...;
};

template<class... Ts>
visitor(Ts...) -> visitor<Ts...>;

constexpr char BACKSLASH = '\\';
constexpr char DOUBLE_QUOTE = '"';
constexpr char SINGLE_QUOTE = '\'';
constexpr char WHITESPACE = ' ';
constexpr char NEWLINE = '\n';
constexpr char REDIRECT_CHAR = '>';

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

        if (std::filesystem::exists(full_path)
            && (std::filesystem::status(full_path).permissions()
                & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) {
            return full_path;
        }
    }

    return std::nullopt;
}

struct EchoCmd {
    const std::string name = "echo";
    const bool is_builtin = true;

    [[nodiscard]] std::optional<int> run(std::ostream &out_os, std::ostream &err_os,
                                         const std::vector<std::string> &args) const {
        for (const auto &s: args) {
            std::print(out_os, "{} ", s);
        }

        std::println(out_os, "");

        return std::nullopt;
    }
};

struct PwdCmd {
    const std::string name = "pwd";
    const bool is_builtin = true;

    [[nodiscard]] std::optional<int> run(std::ostream &out_os, std::ostream &err_os,
                                         const std::vector<std::string> &args) const {
        if (!args.empty())
            std::println(err_os, "pwd: too many arguments");
        else
            std::println(out_os, "{}", std::filesystem::current_path().string());

        return std::nullopt;
    }
};

struct ExitCmd {
    const std::string name = "exit";
    const bool is_builtin = true;

    [[nodiscard]] std::optional<int> run(std::ostream &out_os, std::ostream &err_os,
                                         const std::vector<std::string> &args) const {
        return 0;
    }
};

class CdCmd {
public:
    const std::string name = "cd";
    const bool is_builtin = true;

    [[nodiscard]] std::optional<int> run(std::ostream &out_os, std::ostream &err_os,
                                         const std::vector<std::string> &args) const {
        if (args.empty() || args.front() == "~")
            std::filesystem::current_path(std::getenv("HOME"));
        else if (const auto &arg = args.front(); std::filesystem::exists(arg))
            std::filesystem::current_path(arg);
        else
            std::println(err_os, "cd: {}: No such file or directory", arg);

        return std::nullopt;
    }
};

class TypeCmd {
public:
    const std::string name = "type";
    const bool is_builtin = true;

    [[nodiscard]] std::optional<int> run(std::ostream &out_os, std::ostream &err_os,
                                         const std::vector<std::string> &args) const {
        for (const auto &token: args) {
            if (token == "echo" || token == "cd" || token == "pwd" || token == "exit" || token == "type") {
                std::println(out_os, "{} is a shell builtin", token);
                continue;
            }

            if (const auto maybe_path = find_command_in_path_env_var(token); maybe_path.has_value()) {
                std::println(out_os, "{} is {}", token, maybe_path.value());
                continue;
            }

            std::println(out_os, "{}: not found", token);
        }

        return std::nullopt;
    }
};

class ExternalCmd {
public:
    const std::string name;
    const bool is_builtin = false;
    const std::string program_path;

    explicit ExternalCmd(std::string name, std::string program_path) : name(std::move(name)),
                                                                       program_path(std::move(program_path)) {
    }

    [[nodiscard]] std::optional<int> run(const std::optional<std::string> &out_file,
                                         const std::optional<std::string> &err_file,
                                         const std::vector<std::string> &args) const {
        std::vector<char *> args_copy;

        args_copy.push_back(const_cast<char *>(program_path.c_str()));
        for (const auto &arg: args)
            args_copy.push_back(const_cast<char *>(arg.c_str()));

        args_copy.push_back(nullptr);

        const pid_t pid = fork();
        if (pid < 0)
            return std::nullopt;
        if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
        } else {
            if (out_file.has_value()) {
                const auto &file_name = out_file.value();
                const int file_descriptor = open(file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                                                 S_IRUSR | S_IWUSR);
                if (file_descriptor < 0) {
                    _exit(1);
                }
                if (dup2(file_descriptor, STDOUT_FILENO) < 0) {
                    _exit(1);
                }

                close(file_descriptor);
            }

            if (err_file.has_value()) {
                const auto &file_name = err_file.value();
                const int file_descriptor = open(file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                                                 S_IRUSR | S_IWUSR);
                if (file_descriptor < 0) {
                    _exit(1);
                }
                if (dup2(file_descriptor, STDERR_FILENO) < 0) {
                    _exit(1);
                }

                close(file_descriptor);
            }

            if (execvp(program_path.c_str(), args_copy.data()) == -1) {
                _exit(1);
            }
        }

        return std::nullopt;
    }
};

using Command = std::variant<EchoCmd, CdCmd, PwdCmd, ExitCmd, TypeCmd, ExternalCmd>;

class CommandFactory {
public:
    static std::optional<Command> create_command(const std::string &name) {
        if (name == "echo")
            return EchoCmd();
        if (name == "cd")
            return CdCmd();
        if (name == "pwd")
            return PwdCmd();
        if (name == "exit")
            return ExitCmd();
        if (name == "type")
            return TypeCmd();

        if (const auto program_path = find_command_in_path_env_var(name); program_path.has_value()) {
            return ExternalCmd{name, program_path.value()};
        }

        return std::nullopt;
    }
};


std::ostream &get_ostream_out(const std::optional<std::string> &file, std::optional<std::ofstream> &file_holder) {
    if (file.has_value()) {
        file_holder.emplace(file.value(), std::ios::out);
        return *file_holder;
    }
    return std::cout;
}

std::ostream &get_ostream_err(const std::optional<std::string> &file, std::optional<std::ofstream> &file_holder) {
    if (file.has_value()) {
        file_holder.emplace(file.value(), std::ios::out);
        return *file_holder;
    }
    return std::cerr;
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

std::optional<std::string> get_input() {
    std::string input;
    std::getline(std::cin, input);

    if (input.empty())
        return std::nullopt;

    return input;
}

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    while (true) {
        std::print("$ ");
        std::string input;
        std::getline(std::cin, input);

        auto tokens = tokenize_input(input);
        if (tokens.empty()) {
            continue;
        }

        const auto has_redirection_out = std::ranges::any_of(tokens, [](const auto &t) {
            return t == ">" || t == "1>";
        });

        const auto has_redirection_err = std::ranges::any_of(tokens, [](const auto &t) {
            return t == "2>";
        });

        const auto command_args = tokens | std::views::take_while([](const auto &t) {
            return t != ">" && t != "1>" && t != "2>";
        });

        const auto command_args_vec = std::ranges::to<std::vector<std::string> >(command_args | std::views::drop(1));
        const auto &command_name = command_args.front();


        const auto file_name_out = has_redirection_out
                                       ? std::make_optional((tokens
                                                             | std::views::drop_while([](const auto &t) {
                                                                 return t != ">" && t != "1>";
                                                             })
                                                             | std::views::drop(1)).front())
                                       : std::nullopt;

        const auto file_name_err = has_redirection_err
                                       ? std::make_optional((tokens
                                                             | std::views::drop_while([](const auto &t) {
                                                                 return t != "2>";
                                                             })
                                                             | std::views::drop(1)).front())
                                       : std::nullopt;

        std::optional<std::ofstream> file_holder_out;
        std::optional<std::ofstream> file_holder_err;
        auto &out_os = get_ostream_out(file_name_out, file_holder_out);
        auto &out_err = get_ostream_err(file_name_err, file_holder_err);

        if (const auto maybe_cmd = CommandFactory::create_command(command_name); maybe_cmd.has_value()) {
            const auto &command = maybe_cmd.value();

            const auto result = std::visit(
                visitor {
                    [&out_os, &out_err, command_args_vec](const EchoCmd &cmd) {
                        return cmd.run(out_os, out_err, command_args_vec);
                    },
                    [&out_os, &out_err, command_args_vec](const CdCmd &cmd) {
                        return cmd.run(out_os, out_err, command_args_vec);
                    },
                    [&out_os, &out_err, command_args_vec](const PwdCmd &cmd) {
                        return cmd.run(out_os, out_err, command_args_vec);
                    },
                    [&out_os, &out_err, command_args_vec](const ExitCmd &cmd) {
                        return cmd.run(out_os, out_err, command_args_vec);
                    },
                    [&out_os, &out_err, command_args_vec](const TypeCmd &cmd) {
                        return cmd.run(out_os, out_err, command_args_vec);
                    },
                    [&file_name_out, &file_name_err, command_args_vec](const ExternalCmd &cmd) {
                        return cmd.run(file_name_out, file_name_err, command_args_vec);
                    }
                }, command);

            if (result.has_value()) {
                return result.value();
            }

            continue;
        }

        std::println(out_err, "{}, command not found", command_name);
    }
}

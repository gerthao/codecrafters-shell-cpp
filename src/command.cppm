//
// Created by gthao on 5/11/2026.
//

module;
#include <ostream>
#include <string>
#include <optional>
#include <vector>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>
#include <utility>
#include <fcntl.h>
#include <ranges>

export module Command;

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

export struct EchoCmd {
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

export struct PwdCmd {
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

export struct ExitCmd {
    const std::string name = "exit";
    const bool is_builtin = true;

    [[nodiscard]] std::optional<int> run(std::ostream &out_os, std::ostream &err_os,
                                         const std::vector<std::string> &args) const {
        return 0;
    }
};

export class CdCmd {
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

export class TypeCmd {
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

export class ExternalCmd {
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

export using Command = std::variant<EchoCmd, CdCmd, PwdCmd, ExitCmd, TypeCmd, ExternalCmd>;

export class CommandFactory {
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

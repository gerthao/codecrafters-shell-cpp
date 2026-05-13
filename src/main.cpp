#include <iostream>
#include <string>
#include <print>
#include <ranges>
#include <vector>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <fstream>

import Command;

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

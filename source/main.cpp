// stdc++
#include <iostream>
#include <filesystem>
#include <format>
#include <sstream>
#include <unordered_map>

// sys
#include <unistd.h> // mkstemps
#include <sys/mman.h> // mmap
#include <sys/syslimits.h> // PATH_MAX
#include <fcntl.h> // fcntl
#include <stdio.h> // fileno

// application
#include "vizsuss/shell.hpp"

//----------------------------------------------------------------------------------------------------------------------
// Create a temporary file and return both its path and file descriptor
std::pair<std::filesystem::path, int> temp_file() {
    std::string dyldout_template = "/tmp/dyldout_XXXXXX.txt";
    int dyldout_fd = mkstemps(&dyldout_template[0], 4);
    if (dyldout_fd == -1) {
        throw std::runtime_error(std::format("temp_file mkstemps {}", strerror(errno)));
    }
    return std::make_pair(dyldout_template, dyldout_fd);
}

//----------------------------------------------------------------------------------------------------------------------
// read the contents of a file to string given its file descriptor
std::string file_to_string(int fd) {
    off_t size = lseek(fd, 0, SEEK_END);
    void* contents = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
    if (contents == reinterpret_cast<void*>(-1)) {
        throw std::runtime_error(std::format("file_to_string mmap {}", strerror(errno)));
    }
    std::string result;
    result.resize(size, 0);
    std::memcpy(&result[0], contents, size);
    if (munmap(contents, size) == -1) {
        throw std::runtime_error(std::format("file_to_string munmap {}", strerror(errno)));
    }
    return result;
}

std::string file_to_string(const std::filesystem::path& file) {
    std::unique_ptr<FILE, int(*)(FILE*)> stream(fopen(file.string().c_str(), "rb"), &fclose);
    return file_to_string(fileno(stream.get()));
}

//----------------------------------------------------------------------------------------------------------------------

std::vector<std::string_view> split(const std::string_view& s, const char delimiter) {
    std::vector<std::string_view> result;
    const std::size_t size = s.size();
    std::size_t pos{0};
    while (pos < size) {
        const auto foundpos = s.find(delimiter, pos);
        const auto endpos = foundpos == std::string::npos ? size : foundpos;
        result.push_back(s.substr(pos, endpos - pos));
        pos = endpos + 1;
    }
    return result;
}

//----------------------------------------------------------------------------------------------------------------------

std::string join(const std::vector<std::string_view>& parts, const std::string_view& joiner) {
    std::string result;
    bool first = true;
    for (const auto& part : parts) {
        if (first) {
            first = false;
        } else {
            result += joiner;
        }
        result += part;
    }
    return result;
}

//----------------------------------------------------------------------------------------------------------------------

std::string demangle(const std::string_view& mangled) {
    return vzss::shell(std::format("echo {} | c++filt", mangled));
}

//----------------------------------------------------------------------------------------------------------------------

std::string_view bind_image(const std::string_view& token) {
    // expecting a pattern of `<image/bind#N>`, return `image`
    const auto found = token.find("/bind#");
    if (found == std::string::npos) {
        throw std::runtime_error("token is not a bind image");
    }
    return token.substr(1, found - 1);
}

//----------------------------------------------------------------------------------------------------------------------

std::string human_size(std::size_t size, bool expanded = true) {
    if (size == 0) {
        return "0 bytes";
    }

    auto expanded_amount = [size, expanded]() -> std::string {
        if (!expanded) {
            return "";
        } else {
            return std::format(" ({} bytes)", size);
        }
    };

    switch (static_cast<int>(std::floor(std::log10(size)))) {
        case 3:
        case 4:
        case 5: {
            return std::format("{:.2f} KiB", size / 1024.0) + expanded_amount();
        }
        case 6:
        case 7:
        case 8: {
            return std::format("{:.2f} MiB", size / (1024.0 * 1024.0)) + expanded_amount();
        }
        case 9:
        case 10:
        case 11: {
            return std::format("{:.2f} GiB", size / (1024.0 * 1024.0 * 1024.0)) + expanded_amount();
        }
        default: {
            return std::format("{} bytes", size);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

void process_dyld_output(const std::string& output) {
    std::cout << std::format("dyldout size {}\n", human_size(output.size()));

    const auto lines = split(output, '\n');

    // to regex, or not to regex? That is the question.
    // example: `dyld[80935]: <vistest/bind#7> -> 0x7ff80c4fd38f (libc++abi.dylib/__ZnwmRKSt9nothrow_t)`

    std::unordered_map<std::string_view, std::string> bind_map;

    std::regex

    for (const auto& line : lines) {
        const auto tokens = split(line, ' ');
        if (tokens.size() >= 5 && tokens[1].find("/bind#") != std::string::npos) {
            std::string_view address = tokens[3];
            std::string_view symbol = tokens[4];
            // the symbol is wrapped in parens, so pop those.
            std::string demangled = demangle(symbol.substr(1, symbol.size() - 2));
            // std::cout << "rebound " << demangled << '\n';
            const auto found = bind_map.find(address);
            if (found != bind_map.end()) {
                std::cout << std::format("bind: {} uses {}\n", bind_image(tokens[1]), bind_map[address]);
            } else {
                std::cout << std::format("bind: {} gets {}\n", bind_image(tokens[1]), demangled);
                bind_map[address] = std::move(demangled);
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

std::string dyld_wrapped_run(const std::filesystem::path& input) {
    std::filesystem::path dyldout_path;
    int dyldout_fd{0};
    std::tie(dyldout_path, dyldout_fd) = temp_file();
    const std::string dyldout_path_string = dyldout_path.string();

    // See https://www.manpagez.com/man/1/dyld/
    // Apple `dlyd` source: https://github.com/opensource-apple/dyld/blob/master/src/dyld.cpp
    const std::tuple<const char*, const char*> dyld_environment[] = {
        {"DYLD_PRINT_BINDINGS", "1"},
        // {"DYLD_PRINT_RPATHS", "1"},
        // {"DYLD_PRINT_STATISTICS", "1"},
        {"DYLD_PRINT_TO_FILE", dyldout_path_string.c_str()},
    };

    std::cout << std::format("dyldout: {}\n", dyldout_path_string);

    const std::string input_escaped = join(split(input.string(), ' '), "\\ ");
    std::string command;

    // to account for running `.app`s that are actually directories and require `open` to run.
    if (is_directory(input)) {
        command = "open -W -n " + input_escaped;

        for (const auto envvar : dyld_environment) {
            command += std::string(" --env ") + std::get<0>(envvar) + "="+ std::get<1>(envvar);
        }
    } else {
        for (const auto envvar : dyld_environment) {
            command += std::string(" ") + std::get<0>(envvar) + "="+ std::get<1>(envvar);
        }

        command += " " + input_escaped;
    }

    std::cout << "Waiting for application to exit...\n";
    system(command.c_str());

    return file_to_string(dyldout_fd);
}

//----------------------------------------------------------------------------------------------------------------------

void suss_one_file(const std::filesystem::path& input) {
    std::cout << std::format("Sussing `{}`...\n", input.filename().string());

    std::string dyld_output;

    if (input.filename().extension() == ".txt") {
        // assume this is a file full of dyld output; skip `dyld_wrapped_run`.
        dyld_output = file_to_string(input);
    } else {
        dyld_output = dyld_wrapped_run(input);
    }

    process_dyld_output(dyld_output);
}

//----------------------------------------------------------------------------------------------------------------------

int main(int argc, char** argv) try {
    if (argc != 2) {
        const auto binary_name = std::filesystem::path(argv[0]).filename().string();
        throw std::runtime_error(std::format("{} needs exactly one binary to suss\n", binary_name));
    }

    std::filesystem::path input(argv[1]);

    if (!exists(input)) {
        throw std::logic_error(std::format("binary {} does not exist", input.string()));
    }

    suss_one_file(input);
} catch (const std::exception& error) {
    std::cerr << std::format("Fatal error: {}\n", error.what());
} catch (...) {
    std::cerr << "Fatal error: unknown\n";
}

// identity
#include "vizsuss/shell.hpp"

// stdc++
#include <iostream>

//==========================================================================================================================================

namespace vzss {

//==========================================================================================================================================

std::string shell(const std::string& cmd) {
    FILE* fp = nullptr;
    const char* command = cmd.c_str();
    std::string result;

    fflush(nullptr);
    fp = popen(command, "r");
    if (!fp) {
        std::cerr << "Cannot execute command: " << command << "\n";
        return result;
    }

    char* line = nullptr;
    size_t len = 0;
    while (getline(&line, &len, fp) != -1) {
        result += std::string(line);
    }
    // getline will realloc the pointer as necessary;
    // once we're done with all the getline calls,
    // we can free up the remaining pointer we have.
    free(line);

    fflush(fp);
    if (pclose(fp)) {
        perror("Cannot close stream.\n");
    }

    while (!result.empty() && std::isspace(result.back())) {
        result.pop_back();
    }

    return result;
}

//==========================================================================================================================================

} // namespace vzss

//==========================================================================================================================================

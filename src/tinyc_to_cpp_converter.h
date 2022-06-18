#include <iostream>
#include <fstream>

namespace tinycToCpp {
    std::string read_file(const std::string & filename) {
        std::ifstream input;
        input.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        input.open(filename);
        std::string content;
        char c;
        while (input.get(c)) {
            content.push_back(c);
        }
        return content;
    }

    void find_and_replace(std::string& content, const std::string& keyword, const std::string& replacement) {
        auto index = content.find(keyword);
        while (index != std::string::npos) {
            content.replace(index, keyword.length(), replacement);
            index = content.find(keyword, index);
        }
    }

    void execute(const std::string & filename) {
        auto content = read_file(filename);
        find_and_replace(content, "this", "_this");
        find_and_replace(content, "cast<", "reinterpret_cast<");
        find_and_replace(content, "//CPP:", " ");
        std::cout << content << std::endl;
    }
}
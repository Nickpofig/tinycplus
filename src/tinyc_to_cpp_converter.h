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
        // tinyC code can be a strict version of C++ if few changes to the outputed code applies:
        // 1. tinyC+ extensively uses "this" keyword which means nothing in tinyC, but is a keword in C++.
        //    Therefore, to resolve compile errors on using "this" keyword, we can prefix underscore to it turning it into a valid identifier 
        find_and_replace(content, "this", "_this");
        // 2. C++ does not have "cast" operator, but "reinterpret_cast" matches expectations of what "cast" does,
        find_and_replace(content, "cast<", "reinterpret_cast<");
        // Now, out tinyC code is actually a very modest version of C++ program, which can be runned just fine.
        // [!] To debug the resulted C++ program, please setup project and debug code.
        //     Unfortunetly, there is no print function, and adding it to the language as weird preprocessor is pain in a** to do and explain in thesis.
        std::cout << content << std::endl;

        // [funfact]:
        // it is actually easy to get C++ from tinyC than C, because of complex types
        // where C requires "struct" keyword being used in declarations.
        // Also, "cast" might be a problem to replace as I dont recall what analog C has for it.
    }
}
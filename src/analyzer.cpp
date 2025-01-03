#include <filesystem>
#include <chrono>

#include <parser/parser.hpp>
#include <lexer/lexer.hpp>
//#include <vm/vm.hpp>

#include <console.hpp>
#include <uva/file.hpp>
#include <interpreter/interpreter.hpp>
#include <extension/extension.hpp>
#include <preprocessor/preprocessor.hpp>

using namespace uva;

std::shared_ptr<uva::lang::object> application;
//std::vector<uva::lang::extension*> extensions;
size_t num_linter_warnings = 0;

void write_linter_warning(std::string_view type, std::string_view message, std::string_view file_name, uva::lang::lexer::token_position start, size_t length)
{
    if(num_linter_warnings) {
        std::cout << ",\n";
    }
    std::cout << "\t\t{\n\t\t\t\"type\": \"";
    std::cout << type;
    std::cout << "\",\n\t\t\t\"message\": \"";
    std::cout << message;
    std::cout << "\",\n\t\t\t\"location\": {\n\t\t\t\t\"file\": \"";
    std::cout << file_name;
    std::cout << "\",\n\t\t\t\t\"line\": ";
    std::cout << start.line;
    std::cout << ",\n\t\t\t\t\"column\": ";
    std::cout << start.column;
    std::cout << ",\n\t\t\t\t\"offset\": ";
    std::cout << start.offset;
    std::cout << ",\n\t\t\t\t\"length\": ";
    std::cout << length;
    std::cout << "\n\t\t\t}\n\t\t}";
    ++num_linter_warnings;
}

int main(int argc, char** argv) {
    std::vector<std::string_view> args;
    args.reserve(argc);

    bool is_server = false;

    for(int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];

        if(arg.starts_with("--")) {
            if(arg == "--server") {
                is_server = true;
            }
        } else {
            args.push_back(arg);
        }
    }

    if(is_server) {
        if(args.size() > 0) {
            std::cerr << "uvalang-analyzer --server takes no arguments. Write <input-file>\\n<temp-file>\\n to stdin" << std::endl;
            return 1;
        }
    } else {
        if(args.size() != 1 && args.size() != 2) {
            std::cerr << "uvalang-analyzer <input-file> [temp-file] or uvalang-analyzer --server" << std::endl;
            return 1;
        }
    }

    std::string arg0;
    std::string arg1;

    if(!is_server) {
        arg0 = args[0];

        if(args.size() > 1) {
            arg1 = args[1];
        } else {
            arg1 = arg0;
        }
    }

    bool run = true;

    while(run) {
        num_linter_warnings = 0;
        std::filesystem::path uva_executable_path = argv[0];

        if(is_server) {
            std::getline(std::cin, arg0);
            std::getline(std::cin, arg1);
        } else {
            run = false;
        }

        std::filesystem::path file_path = std::filesystem::absolute(arg0);
        std::filesystem::path temp_file_path = std::filesystem::absolute(arg1);

        auto start = std::chrono::high_resolution_clock::now();

        if(!std::filesystem::exists(file_path)) {
            std::cerr << "input file '" << file_path.string() << "' does not exist" << std::endl;
            exit(1);
        }

        if(!std::filesystem::is_regular_file(file_path)) {
            std::cerr << "input file '" << file_path.string() << "' is not a regular file" << std::endl;
            exit(1);
        }

        std::string source = uva::file::read_all_text<char>(temp_file_path);

        uva::lang::lexer l;

        try {
            l.tokenize(file_path.string(), source);
        } catch (const std::exception& e) {
            (void)e;
        }
        
        uva::lang::preprocessor preprocessor;
        preprocessor.process(file_path, l);

        // Note we are writing directly to the cout instead of saving and encoding the output

        std::cout << "{\n";
        std::cout << "\t\"linter\": [\n";

        for(const auto& token : l.tokens()) {
            switch(token.type()) {
                case uva::lang::lexer::token_type::token_literal:
                    switch(token.kind()) {
                        case uva::lang::lexer::token_kind::token_string:
                            size_t offset = token.start.offset;
                            char c;

                            if(token.m_file_name != file_path.string()) {
                                //TODO: save all files in lexer
                                std::ifstream file(token.m_file_name);
                                file.seekg(offset);
                                file.read(&c, 1);
                            } else {
                                c = source[offset];
                            }

                            switch(c)
                            {
                                case '\"':
                                    if(token.content().find("${") == std::string::npos) {
                                        write_linter_warning("string-default-single-quotes", "String literal without interpolation should use single quotes", token.m_file_name, token.start, token.content().size() + 2 /* 2 for the quotes */);
                                    }
                                break;
                            }
                        break;
                    }
                break;
            }
        }

        std::cout << "\n\t],\n";

        std::cout << "\t\"declarations\": [";

        uva::lang::parser p;
        uva::lang::parser::ast_node root_node;
        
        try {
            root_node = p.parse_all(l);
        } catch (const std::exception& e) {
            (void)e;
        }

        size_t node_i = 0;

        for(const auto& node : root_node.childrens()) {
            
            if(node.type() == uva::lang::parser::ast_node_type::ast_node_classdecl) {
                if(node_i) {
                    std::cout << ",";
                }

                node_i++;

                std::cout << "\n";

                const uva::lang::parser::ast_node* decname_node = node.child_from_type(uva::lang::parser::ast_node_type::ast_node_declname);
                const uva::lang::lexer::token& decname_token = decname_node->token();

                std::cout << "\t\t{\n\t\t\t\"type\": \"class\",\n\t\t\t\"name\": \"";
                std::cout << decname_token.content();
                std::cout << "\",\n\t\t\t\"location\": {\n\t\t\t\t\"file\": \"";
#ifdef __UVA_WIN__
                for(const auto& c : decname_token.m_file_name) {
                    std::cout << c;
                    if(c == '\\') {
                        std::cout << "\\";
                    }
                }
#else
                std::cout << decname_token.m_file_name;
#endif
                std::cout << "\",\n";
                std::cout << "\t\t\t\t\"line\": ";
                std::cout << decname_token.start.line;
                std::cout << ",\n\t\t\t\t\"column\": ";
                std::cout << decname_token.start.column;
                std::cout << ",\n\t\t\t\t\"offset\": ";
                std::cout << decname_token.start.offset;
                std::cout << "\n\t\t\t}";
                std::cout << ",\n\t\t\t\"references\": [";

                size_t token_i = 0;

                for(const auto& token : l.tokens()) {
                    if(token.type() == uva::lang::lexer::token_type::token_identifier) {
                        if(token.content() == decname_token.content()) {
                            if(token_i) {
                                std::cout << ",";
                            }

                            token_i++;

                            std::cout << "\n\t\t\t\t{\n\t\t\t\t\t\"file\": \"";
#ifdef __UVA_WIN__
                            for(const auto& c : token.m_file_name) {
                                std::cout << c;
                                if(c == '\\') {
                                    std::cout << "\\";
                                }
                            }
#else
                            std::cout << token.m_file_name;
#endif
                            std::cout << "\",\n\t\t\t\t\t\"line\": ";
                            std::cout << token.start.line;
                            std::cout << ",\n\t\t\t\t\t\"column\": ";
                            std::cout << token.start.column;
                            std::cout << ",\n\t\t\t\t\t\"offset\": ";
                            std::cout << token.start.offset;
                            std::cout << "\n\t\t\t\t}";
                        }
                    }
                }

                std::cout << "\n\t\t\t]";
                std::cout << "\n\t\t}";
            }
        }

        std::cout << "\n\t],\n";

        auto end = std::chrono::high_resolution_clock::now();

        std::cout << "\t\"elapsed\": \"" << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms\"\n";

        std::cout << "}";
    }

    return 0;
}
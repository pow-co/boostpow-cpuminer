#include <stratum.hpp>
#include <boost/program_options.hpp>

int command_connect (int arg_count, char** arg_values) {
    throw data::exception {"function doesn't function"};
}

int command_serve (int arg_count, char** arg_values) {
    throw data::exception {"function doesn't function"};
}

int help() {

    std::cout << "helllllp! " << std::endl;
    
    return 0;
}

int main(int arg_count, char** arg_values) {
    if(arg_count == 1) return help();
    //if (arg_count != 5) return help();
    
    string function{arg_values[1]};
    
    try {
        if (function == "connect") return command_connect(arg_count - 1, arg_values + 2);
        if (function == "serve") return command_connect(arg_count - 1, arg_values + 2);
        if (function == "help") return help();
        help();
        
    } catch (std::string x) {
        std::cout << "Error: " << x << std::endl;
        return 1;
    }
    
    return 0;
}


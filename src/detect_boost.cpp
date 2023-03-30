
#include <data/encoding/hex.hpp>
#include <gigamonkey/timechain.hpp>
#include <jobs.hpp>

using namespace Gigamonkey;

int main(int arg_count, char** args) {
    
    try {
        
        if (arg_count != 2) throw data::exception {"Invalid number of arguments; expected one argument."};
        
        maybe<bytes> read_hex = encoding::hex::read (std::string{args[1]});
        
        if (!bool (read_hex)) throw data::exception {"expected hex string"};
        
        auto tx = Bitcoin::transaction {*read_hex};
        if (!tx.valid()) throw data::exception {"could not read transaction"};
        
        for (auto &o : tx.Outputs) if (Boost::output_script {o.Script}.valid ()) {
            std::cout << BoostPOW::write (tx.id ()).substr (2) << std::endl;
            return 0;
        }
        
    } catch (data::exception &e) {
        std::cout << "error: " << e.what () << std::endl;
        return 1;
    } catch (...) {
        std::cout << "error: unknown" << std::endl;
        return 1;
    }
    
    return 0;
};


#include <network.hpp>

bool network::broadcast(const bytes &tx) {
    WhatsOnChain.transaction().broadcast(tx);
    Gorilla.submit_transaction({tx});
    
    std::cout << "Attempted to broadcast transaction." << std::endl;
    return true;
}


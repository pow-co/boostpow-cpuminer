#include <whatsonchain_api.hpp>

bool whatsonchain::transactions::broadcast(const bytes &tx) {
    
    std::cout << "## broadcasting tx " << tx << std::endl;
    
    auto request = API.Rest.POST("/v1/bsv/main/tx/raw", 
        {{networking::HTTP::header::content_type, "application/json"}}, 
        json{{"tx_hex", encoding::hex::write(tx)}}.dump());
    
    auto response = API(request);
    
    std::cout << "## status code is " << static_cast<unsigned int>(response.Status) << ": " << response.Status << std::endl;
    std::cout << "## headers are " << response.Headers << std::endl;
    std::cout << "## response body is " << response.Body << std::endl;
    
    if (static_cast<unsigned int>(response.Status) >= 500) 
        throw networking::HTTP::exception{request, response, string{"problem reading txid."}};
    
    if (static_cast<unsigned int>(response.Status) != 200 || 
        response.Headers[networking::HTTP::header::content_type] == "text/plain") {
        std::cout << "!!!! failed to broadcast tx. Status: " << response.Status << "; Body: " << response.Body << std::endl;
        return false;
    }
    
    if (response.Body == "") return false;
    
    return true;
}
        
whatsonchain::utxo::utxo() : Outpoint{}, Value{}, Height{} {}

whatsonchain::utxo::utxo(const json &item) : utxo{} {
    
    digest256 tx_hash{string{"0x"} + string(item.at("tx_hash"))};
    if (!tx_hash.valid()) return;
    
    Outpoint = Bitcoin::outpoint{tx_hash, uint32(item.at("tx_pos"))};
    Value = Bitcoin::satoshi{int64(item.at("value"))};
    Height = uint32(item.at("height"));
    
}

list<whatsonchain::utxo> whatsonchain::addresses::get_unspent(const Bitcoin::address &addr) {
    std::stringstream ss;
    ss << "/v1/bsv/main/address/" << addr << "/unspent";
    auto response = API.GET(ss.str());
    
    if (response.Status != networking::HTTP::status::ok || 
        response.Headers[networking::HTTP::header::content_type] != "application/json") throw response;
    
    json info = json::parse(response.Body);
    
    if (!info.is_array()) throw response;
    
    list<utxo> utxos;
    
    for (const json &item : info) {
        
        utxo u(item);
        if (!u.valid()) throw response;
        
        utxos = utxos << u;
        
    }
    
    return utxos;
}

list<whatsonchain::utxo> whatsonchain::scripts::get_unspent(const digest256 &script_hash) {
    std::stringstream ss;
    ss << script_hash;
    
    string call = string{"/v1/bsv/main/script/"} + ss.str().substr(9, 64) + "/unspent";
    
    auto request = API.Rest.GET(call);
    auto response = API(request);
    
    if (response.Status != networking::HTTP::status::ok) 
        throw networking::HTTP::exception{request, response, string{"response status is not ok. body is: "} + response.Body};
    /*
    if (response.Headers[networking::HTTP::header::content_type] != "application/json") 
        throw networking::HTTP::exception{request, response, "response header content_type does not indicate application/json"};
    */
    list<utxo> utxos;
    
    try {
        
        json unspent_utxos_json = json::parse(response.Body);
        
        for (const json &item : unspent_utxos_json) {
            
            utxo u(item);
            if (!u.valid()) throw response;
            
            utxos = utxos << u;
            
        }
    } catch (const json::exception &exception) {
        throw networking::HTTP::exception{request, response, string{"problem reading json: "} + string{exception.what()}};
    }
    
    return utxos;
}

list<Bitcoin::txid> whatsonchain::scripts::get_history(const digest256& script_hash) {
    std::stringstream ss;
    ss << script_hash;
    
    string call = string{"/v1/bsv/main/script/"} + ss.str().substr(9, 64) + string{"/history"};
    
    auto request = API.Rest.GET(call);
    auto response = API(request);
    
    if (response.Status != networking::HTTP::status::ok) 
        throw networking::HTTP::exception{request, response, "response status is not ok"};
    /*
    if (response.Headers[networking::HTTP::header::content_type] != "application/json") 
        throw networking::HTTP::exception{request, response, "response header content_type does not indicate application/json"};
    */
    list<Bitcoin::txid> txids;
    
    try {
        
        json txids_json = json::parse(response.Body);
        
        for (const json &item : txids_json) {
            
            Bitcoin::txid txid{string{"0x"} + string(item.at("tx_hash"))};
            if (!txid.valid()) throw response;
            
            txids = txids << txid;
            
        }
    } catch (const json::exception &exception) {
        throw networking::HTTP::exception{request, response, string{"problem reading json: "} + string{exception.what()}};
    }
    
    return txids;
}

bytes whatsonchain::transactions::get_raw(const Bitcoin::txid &txid) {
    static map<Bitcoin::txid, bytes> cache;
    
    auto known = cache.contains(txid);
    if (known) return *known;
    
    std::stringstream ss;
    ss << txid;
    
    string call = string{"/v1/bsv/main/tx/"} + ss.str().substr(9, 64) + string{"/hex"};
    
    auto request = API.Rest.GET(call);
    auto response = API(request);
    
    if (static_cast<unsigned int>(response.Status) == 404) return {};
    
    if (response.Status != networking::HTTP::status::ok) 
        throw networking::HTTP::exception{request, response, "response status is not ok"};
    
    ptr<bytes> tx = encoding::hex::read(response.Body);
    
    if (tx == nullptr) return {};
    cache = cache.insert(txid, *tx);
    return *tx;
}

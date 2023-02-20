#include <whatsonchain_api.hpp>

bool whatsonchain::transactions::broadcast(const bytes &tx) {
    
    auto request = API.REST.POST ("/v1/bsv/main/tx/raw",
        {{net::HTTP::header::content_type, "application/JSON"}},
        JSON {{"tx_hex", encoding::hex::write (tx)}}.dump ());
    
    auto response = API (request);
    
    if (static_cast<unsigned int> (response.Status) >= 500)
        throw net::HTTP::exception {request, response, string {"problem reading txid."}};
    
    if (static_cast<unsigned int> (response.Status) != 200 ||
        response.Headers[net::HTTP::header::content_type] == "text/plain") return false;
    
    if (response.Body == "") return false;
    
    return true;
}
        
UTXO::UTXO () : Outpoint {}, Value {}, Height {} {}

UTXO::UTXO (const JSON &item) : UTXO {} {
    
    digest256 tx_hash {string {"0x"} + string (item.at ("tx_hash"))};
    if (!tx_hash.valid ()) return;
    
    Outpoint = Bitcoin::outpoint {tx_hash, uint32 (item.at ("tx_pos"))};
    Value = Bitcoin::satoshi {int64 (item.at ("value"))};
    Height = uint32 (item.at ("height"));
    
}

UTXO::operator JSON() const {
    std::stringstream ss;
    ss << Outpoint.Digest;
    return JSON {
        {"tx_hash", ss.str ().substr(9, 64)},
        {"tx_pos", Outpoint.Index}, 
        {"value", int64 (Value)},
        {"height", Height}};
}

list<UTXO> whatsonchain::addresses::get_unspent (const Bitcoin::address &addr) {
    std::stringstream ss;
    ss << "/v1/bsv/main/address/" << addr << "/unspent";
    auto request = API.REST.GET (ss.str ());
    auto response = API (request);
    
    if (response.Status != net::HTTP::status::ok) {
        std::stringstream z;
        z << "status = \"" << response.Status << "\"; content_type = " << 
            response.Headers[net::HTTP::header::content_type] << "; body = \"" << response.Body << "\"";
        throw net::HTTP::exception {request, response, z.str ()};
    }
    
    JSON info = JSON::parse (response.Body);
    
    if (!info.is_array ()) throw response;
    
    list<UTXO> UTXOs;
    
    for (const JSON &item : info) {
        
        UTXO u (item);
        if (!u.valid ()) throw response;
        
        UTXOs = UTXOs << u;
        
    }
    
    return UTXOs;
}

list<UTXO> whatsonchain::scripts::get_unspent (const digest256 &script_hash) {
    std::stringstream ss;
    ss << script_hash;
    
    string call = string {"/v1/bsv/main/script/"} + ss.str ().substr (9, 64) + "/unspent";
    
    auto request = API.REST.GET (call);
    auto response = API (request);
    
    if (response.Status != net::HTTP::status::ok)
        throw net::HTTP::exception {request, response, string {"response status is not ok. body is: "} + response.Body};
    /*
    if (response.Headers[networking::HTTP::header::content_type] != "application/JSON") 
        throw networking::HTTP::exception{request, response, "response header content_type does not indicate application/JSON"};
    */
    list<UTXO> UTXOs;
    
    try {
        
        JSON unspent_UTXOs_JSON = JSON::parse (response.Body);
        
        for (const JSON &item : unspent_UTXOs_JSON) {
            
            UTXO u (item);
            if (!u.valid ()) throw response;
            
            UTXOs = UTXOs << u;
            
        }
    } catch (const JSON::exception &exception) {
        throw net::HTTP::exception {request, response, string {"problem reading JSON: "} + string {exception.what()}};
    }
    
    return UTXOs;
}

list<Bitcoin::txid> whatsonchain::scripts::get_history (const digest256& script_hash) {
    std::stringstream ss;
    ss << script_hash;
    
    string call = string {"/v1/bsv/main/script/"} + ss.str ().substr (9, 64) + string {"/history"};
    
    auto request = API.REST.GET (call);
    auto response = API (request);
    
    if (response.Status != net::HTTP::status::ok)
        throw net::HTTP::exception {request, response, "response status is not ok"};
    /*
    if (response.Headers[networking::HTTP::header::content_type] != "application/JSON") 
        throw networking::HTTP::exception{request, response, "response header content_type does not indicate application/JSON"};
    */
    list<Bitcoin::txid> txids;
    
    try {
        
        JSON txids_JSON = JSON::parse (response.Body);
        
        for (const JSON &item : txids_JSON) {
            
            Bitcoin::txid txid {string{"0x"} + string (item.at ("tx_hash"))};
            if (!txid.valid ()) throw response;
            
            txids = txids << txid;
            
        }
    } catch (const JSON::exception &exception) {
        throw net::HTTP::exception {request, response, string {"problem reading JSON: "} + string {exception.what ()}};
    }
    
    return txids;
}

bytes whatsonchain::transactions::get_raw (const Bitcoin::txid &txid) {
    std::stringstream ss;
    ss << txid;
    
    string call = string {"/v1/bsv/main/tx/"} + ss.str ().substr (9, 64) + string {"/hex"};
    
    auto request = API.REST.GET (call);
    auto response = API (request);
    
    if (static_cast<unsigned int> (response.Status) == 404) return {};
    
    if (response.Status != net::HTTP::status::ok)
        throw net::HTTP::exception {request, response, "response status is not ok"};
    
    ptr<bytes> tx = encoding::hex::read (response.Body);
    
    if (tx == nullptr) return {};
    return *tx;
}

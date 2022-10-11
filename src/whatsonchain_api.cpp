#include <whatsonchain_api.hpp>

bool whatsonchain::transactions::broadcast(const bytes &tx) {
    
    auto response = API.POST("/v1/bsv/main/tx/raw", 
        {{networking::HTTP::header::content_type, "application/json"}}, 
        json{{"tx_hex", encoding::hex::write(tx)}}.dump());
    
    return static_cast<unsigned int>(response.Status) == 200;
}


    struct utxo {
        
        Bitcoin::txid TxHash;
        uint32 Index;
        Bitcoin::satoshi Value;
        uint32 Height;
        
whatsonchain::utxo::utxo() : TxHash{}, Index{}, Value{}, Height{} {}

whatsonchain::utxo::utxo(const json &item) : utxo{} {
    
    if (!item.contains("height") && 
        !item.contains("tx_pos") && 
        !item.contains("tx_hash") && 
        !item.contains("value")) return;
    
    auto height = item["height"];
    auto tx_pos = item["tx_pos"];
    auto tx_hash_hex = item["tx_hash"];
    auto value = item["value"];
    
    if (!height.is_number_unsigned() && 
        !tx_pos.is_number_unsigned() && 
        !value.is_number_unsigned() && 
        !tx_hash_hex.is_string()) return;
    
    digest256 tx_hash{string{"0x"} + string(tx_hash_hex)};
    if (!tx_hash.valid()) return;
    
    Txid = tx_hash;
    Index = uint32(tx_pos); 
    Value = Bitcoin::satoshi{int64(value)};
    Height = uint32(height);
    
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
        
        auto u = utxo_from_json(item);
        if (!u.valid()) throw response;
        
        utxos = utxos << u;
        
    }
    
    return utxos;
}

list<whatsonchain::utxo> whatsonchain::scripts::get_unspent(const digest256 &script_hash) {
    std::stringstream ss;
    ss << script_hash;
    
    auto response = API.GET(string{"/v1/bsv/main/script/"} + script_hash.substr(2) "/unspent");
    
    if (response.Status != networking::HTTP::status::ok || 
        response.Headers[networking::HTTP::header::content_type] != "application/json") throw response;
    
    json info = json::parse(response.Body);
    
    if (!info.is_array()) throw response;
    
    list<utxo> utxos;
    
    for (const json &item : info) {
        
        auto u = utxo_from_json(item);
        if (!u.valid()) throw response;
        
        utxos = utxos << u;
        
    }
    
    return utxos;
}

#include <whatsonchain_api.hpp>

bool whatsonchain::transactions::broadcast(const bytes &tx) {
    
    auto response = API.POST("/v1/bsv/main/tx/raw", 
        {{networking::HTTP::header::content_type, "application/json"}}, 
        json{{"tx_hex", encoding::hex::write(tx)}}.dump());
    
    return static_cast<unsigned int>(response.Status) == 200;
}

list<whatsonchain::addresses::unspent_output_info> 
whatsonchain::addresses::get_unspent_transactions(const Bitcoin::address &addr) {
    std::stringstream ss;
    ss << "/v1/bsv/main/address/" << addr << "/unspent";
    auto response = API.GET(ss.str());
    
    if (static_cast<unsigned int>(response.Status) != 200) throw response;
    
    json info = json::parse(response.Body);
    
    if (!info.is_array()) throw response;
    
    list<whatsonchain::addresses::unspent_output_info> return_data;
    
    for (const json &item : info) {
        if (!item.contains("height") && 
            !item.contains("tx_pos") && 
            !item.contains("tx_hash") && 
            !item.contains("value")) throw response;
        
        auto height = item["height"];
        auto tx_pos = item["tx_pos"];
        auto tx_hash_hex = item["tx_hash"];
        auto value = item["value"];
        
        if (!height.is_number_unsigned() && 
            !tx_pos.is_number_unsigned() && 
            !value.is_number_unsigned() && 
            !tx_hash_hex.is_string()) throw response;
        
        digest256 tx_hash{string{"0x"} + string(tx_hash_hex)};
        
        return_data = return_data << whatsonchain::addresses::unspent_output_info{
            tx_hash, 
            uint32(tx_pos), 
            Bitcoin::satoshi{int64(value)}, 
            uint32(height)};
        
    }
    
    return return_data;
}

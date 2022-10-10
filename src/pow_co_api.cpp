#include <pow_co_api.hpp>

list<Boost::output> pow_co::jobs() {
    auto response = this->GET("/api/v1/boost/jobs");
    
    if (response.Status != networking::HTTP::status::ok || 
        response.Headers[networking::HTTP::header::content_type] != "application/json") throw response;
    
    auto json_jobs = json.parse(response.Body);
    
    if (!jobs.is_array()) throw response;
    
    list<Boost::prevout> jobs;
    for (const auto &job : json_jobs) {
        
        if (!job.contains("txid") || !job.contains("vout") || 
            !job.contains("script") || !job.contains("value")) throw response;
        
        auto txid_hex = job["txid"];
        auto script_hex = job["script"];
        auto value_json = job["value"];
        auto vout_json = job["vout"];
        
        if (!script_hex.is_string() || !txid_hex.is_string() || 
            !value_json.is_number_unsigned() || !vout_json.is_number_unsigned()) throw response;
        
        uint32 index;
        std::stringstream{string(vout_json)} >> index;
        
        int64 value;
        std::stringstream{string(value_json)} >> value;
        
        digest256 txid{string{"0x"} + string(txid_hex)};
        if (!txid.valid()) throw response;
        
        ptr<bytes> script_bytes = encoding::hex::read(string(script_hex));
        if (script_bytes == nullptr) throw response;
        
        Boost::output_script script{*bytes};
        if (!script.valid()) throw response;
        
        jobs = jobs << Boost::prevout{
            Bitcoin::outpoint{txid, index}, 
            Boost::output{Bitcoin::satoshi{value}, script}};
        
    }
    
    return jobs;
}

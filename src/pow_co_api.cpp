#include <pow_co_api.hpp>

list<Boost::prevout> pow_co::jobs(uint32 limit) {
    std::stringstream ss;
    ss << limit;
    
    auto request = this->Rest.GET("/api/v1/boost/jobs", {{"limit", ss.str()}});
    auto response = this->operator()(request);
    
    if (response.Status != networking::HTTP::status::ok) 
        throw networking::HTTP::exception{request, response, "response status is not ok"};
    /*
    if (response.Headers[networking::HTTP::header::content_type] != "application/json") 
        throw networking::HTTP::exception{request, response, "expected content type application/json"};
    */
    
    list<Boost::prevout> boost_jobs;
    try {
        
        json json_jobs = json::parse(response.Body).at("jobs");
        
        for (const json &job : json_jobs) {
            
            uint32 index{job.at("vout")};
            
            int64 value{job.at("value")};
            
            digest256 txid{string{"0x"} + string(job.at("txid"))};
            if (!txid.valid()) throw response;
            
            ptr<bytes> script_bytes = encoding::hex::read(string(job.at("script")));
            if (script_bytes == nullptr) 
                throw networking::HTTP::exception{request, response, "script should be in hex format"};
            
            Boost::output_script script{*script_bytes};
            if (!script.valid()) 
                throw networking::HTTP::exception{request, response, "invalid boost script"};
            
            boost_jobs = boost_jobs << Boost::prevout{
                Bitcoin::outpoint{txid, index}, 
                Boost::output{Bitcoin::satoshi{value}, script}};
            
        }
    } catch (const json::exception &j) {
        throw networking::HTTP::exception{request, response, string{"invalid json format: "} + string{j.what()}};
    }
    
    return boost_jobs;
}

inpoint pow_co::spends(const Bitcoin::outpoint &outpoint) {
    std::stringstream hash_stream;
    hash_stream << outpoint.Digest;
    
    std::stringstream path_stream;
    path_stream << "/api/v1/spends/" << hash_stream.str().substr(9, 64) << "/" << outpoint.Index; 
    
    auto request = this->Rest.GET(path_stream.str());
    auto response = this->operator()(request);
    
    if (response.Status != networking::HTTP::status::ok) 
        throw networking::HTTP::exception{request, response, "response status is not ok"};
    
    list<Boost::prevout> boost_jobs;
    try {
        if (response.Body == "") return {};
        std::cout << "Response is \"" << response.Body << "\"" << std::endl;
    } catch (const json::exception &j) {
        throw networking::HTTP::exception{request, response, string{"invalid json format: "} + string{j.what()}};
    }
    
    return {};
    
}

void pow_co::submit_proof(const Bitcoin::txid &txid) {
    std::stringstream hash_stream;
    hash_stream << txid;
    
    std::stringstream path_stream;
    path_stream << "/api/v1/boost/proofs/" << hash_stream.str().substr(9, 64); 
    
    auto request = this->Rest.POST(path_stream.str());
    this->operator()(request);
}

bool pow_co::broadcast(const bytes &tx) {
    
    auto request = this->Rest.POST("/api/v1/transactions/", 
        {{networking::HTTP::header::content_type, "application/json"}}, 
        json{{"transaction", encoding::hex::write(tx)}}.dump());
    
    auto response = (*this)(request);
    
    if (static_cast<unsigned int>(response.Status) >= 500) 
        throw networking::HTTP::exception{request, response, string{"problem reading txid."}};
    
    if (static_cast<unsigned int>(response.Status) != 200) return false;
    
    try {
        return json::parse(response.Body)["error"] == 0;
    } catch (const json::exception &) {
        return false;
    }
}

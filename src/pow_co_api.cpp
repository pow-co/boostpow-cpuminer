#include <pow_co_api.hpp>
#include <data/net/websocket.hpp>
#include <data/net/JSON.hpp>

Boost::prevout read_job (const JSON &job,
    net::HTTP::request &request,
    net::HTTP::response &response) {
    
    uint32 index {job.at ("vout")};
    
    int64 value {job.at ("value")};
    
    digest256 txid {string {"0x"} + string (job.at ("txid"))};
    if (!txid.valid ()) throw net::HTTP::exception {request, response, "cannot read txid"};
    
    ptr<bytes> script_bytes = encoding::hex::read (string (job.at ("script")));
    if (script_bytes == nullptr) 
        throw net::HTTP::exception {request, response, "script should be in hex format"};
    
    Boost::output_script script {*script_bytes};
    if (!script.valid ())
        throw net::HTTP::exception {request, response, "invalid boost script"};
    
    return Boost::prevout {
        Bitcoin::outpoint {txid, index},
        Boost::output {Bitcoin::satoshi {value}, script}};
}

list<Boost::prevout> pow_co::jobs (uint32 limit) {
    std::stringstream ss;
    ss << limit;
    
    auto request = this->REST.GET ("/api/v1/boost/jobs", {{"limit", ss.str ()}});
    auto response = this->operator () (request);
    
    if (response.Status != net::HTTP::status::ok) {
        std::stringstream ss;
        ss << "response status is " << response.Status;
        throw net::HTTP::exception {request, response, ss.str () };
    }
    /*
    if (response.Headers[net::HTTP::header::content_type] != "application/JSON")
        throw net::HTTP::exception {request, response, "expected content type application/JSON"};
    */
    
    list<Boost::prevout> boost_jobs;
    try {
        
        JSON JSON_jobs = JSON::parse (response.Body).at ("jobs");
        
        for (const JSON &job : JSON_jobs) boost_jobs = boost_jobs << read_job (job, request, response);
    } catch (const JSON::exception &j) {
        throw net::HTTP::exception {request, response, string {"invalid JSON format: "} + string {j.what ()}};
    }
    
    return boost_jobs;
}

inpoint pow_co::spends (const Bitcoin::outpoint &outpoint) {
    std::stringstream hash_stream;
    hash_stream << outpoint.Digest;
    
    std::stringstream path_stream;
    path_stream << "/api/v1/spends/" << hash_stream.str ().substr (9, 64) << "/" << outpoint.Index;
    
    auto request = this->REST.GET (path_stream.str ());
    auto response = this->operator () (request);
    
    if (response.Status != net::HTTP::status::ok) {
        std::stringstream ss;
        ss << "response status is " << response.Status;
        throw net::HTTP::exception {request, response, ss.str () };
    }
    
    list<Boost::prevout> boost_jobs;
    try {
        if (response.Body == "") return {};
    } catch (const JSON::exception &j) {
        throw net::HTTP::exception {request, response, string {"invalid JSON format: "} + string {j.what ()}};
    }
    
    return {};
    
}

void pow_co::submit_proof (const bytes &tx) {
    auto request = this->REST.POST ("/api/v1/boost/proofs",
        {{net::HTTP::header::content_type, "application/JSON"}},
        JSON {{"transaction", encoding::hex::write (tx)}}.dump ());
    this->operator () (request);
}

bool pow_co::broadcast (const bytes &tx) {
    
    auto request = this->REST.POST ("/api/v1/transactions",
        {{net::HTTP::header::content_type, "application/JSON"}},
        JSON {{"transaction", encoding::hex::write (tx)}}.dump ());
    
    auto response = (*this) (request);
    
    if (static_cast<unsigned int> (response.Status) >= 500)
        throw net::HTTP::exception{request, response, string{"problem reading txid."}};
    
    if (static_cast<unsigned int> (response.Status) != 200) {
        std::cout << "pow co returns response code " << response.Status << std::endl;
        return false;
    }
    
    std::cout << " pow co broadcast response body: " << response.Body << std::endl;
    try {
        return !JSON::parse (response.Body).contains ("error");
    } catch (const JSON::exception &) {
        return false;
    }
}

Boost::prevout pow_co::job(const Bitcoin::txid &txid) {
    std::stringstream hash_stream;
    hash_stream << txid;
    
    std::stringstream path_stream;
    path_stream << "/api/v1/boost/jobs/" << hash_stream.str ().substr (9, 64);
    
    auto request = this->REST.GET (path_stream.str ());
    
    auto response = (*this) (request);
    
    if (static_cast<unsigned int> (response.Status) >= 500)
        throw net::HTTP::exception{request, response, string {"problem reading txid."}};
    
    if (static_cast<unsigned int> (response.Status) != 200)
        std::cout << "pow co returns response code " << response.Status << std::endl;
    
    return read_job (JSON::parse (response.Body)["job"], request, response);
}

Boost::prevout pow_co::job (const Bitcoin::outpoint &o) {
    std::stringstream hash_stream;
    hash_stream << o.Digest;
    
    std::stringstream path_stream;
    path_stream << "/api/v1/boost/jobs/" << hash_stream.str ().substr (9, 64) << "_o" << o.Index;
    
    auto request = this->REST.GET (path_stream.str ());
    
    auto response = (*this) (request);
    
    if (static_cast<unsigned int> (response.Status) >= 500)
        throw net::HTTP::exception {request, response, string {"problem reading txid."}};
    
    if (static_cast<unsigned int> (response.Status) != 200)
        std::cout << "pow co returns response code " << response.Status << std::endl;
    std::cout << " response body " << response.Body << std::endl;
    return read_job (JSON::parse (response.Body)["job"], request, response);
}

void pow_co::connect (
        net::asio::error_handler error_handler,
        net::interaction<const JSON &> interact,
        net::close_handler closed) {
    net::open_JSON_session ([] (parse_error err) -> void {
        throw err;
    }, [&io = this->IO, url = net::URL {net::protocol::WS, 5201, this->REST.Host, string {"/"}}, ssl = this->SSL.get (), error_handler]
        (net::close_handler closed, net::interaction<string_view, const string &> interact) -> void {
        net::websocket::open (io, url, ssl, error_handler, closed, interact);
    }, interact, closed);
}

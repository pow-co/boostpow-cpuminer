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
    
    maybe<bytes> script_bytes = encoding::hex::read (string (job.at ("script")));
    if (!bool (script_bytes))
        throw net::HTTP::exception {request, response, "script should be in hex format"};
    
    Boost::output_script script {*script_bytes};
    if (!script.valid ())
        throw net::HTTP::exception {request, response, "invalid boost script"};
    
    return Boost::prevout {
        Bitcoin::outpoint {txid, index},
        Boost::output {Bitcoin::satoshi {value}, script}};
}

list<Boost::prevout> pow_co::jobs (uint32 limit, double max_difficulty) {
    
    list<entry<string, string>> query_params;
    
    query_params <<= entry<string, string> {"limit", std::to_string (limit)};
    
    if (max_difficulty > 0) query_params <<= entry<string, string> {"maxDifficulty", std::to_string (max_difficulty)};
    
    auto request = this->REST.GET ("/api/v1/boost/jobs", query_params);
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

Boost::prevout pow_co::job (const Bitcoin::txid &txid) {
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

bool pow_co::websockets_protocol_message::valid (const JSON &j) {
    return j.is_object () &&
        j.contains ("type") && j["type"].is_string () &&
        j.contains ("content");
}

std::optional<Boost::prevout> pow_co::websockets_protocol_message::job_created (const JSON &j) {
    if (!(j.is_object () &&
        j.contains ("txid") && j["txid"].is_string () &&
        j.contains ("script") && j["script"].is_string () &&
        j.contains ("vout") && j["vout"].is_number_unsigned () &&
        j.contains ("value") && j["value"].is_number_unsigned ())) return {};

    auto hex_decoded = encoding::hex::read (string (j["script"]));
    if (! bool (hex_decoded)) return {};

    auto output_script = Boost::output_script::read (*hex_decoded);
    if (!output_script.valid ()) return {};

    Bitcoin::txid txid {string {"0x"} + string (j["txid"])};
    if (!txid.valid ()) return {};

    return Boost::prevout {
        Bitcoin::outpoint {txid, uint32 (j["vout"])},
        Boost::output {int64 (j["value"]), output_script}};
}

std::optional<Bitcoin::outpoint> pow_co::websockets_protocol_message::proof_created (const JSON &j) {
    if (!(j.is_object () &&
        j.contains ("job_txid") && j["job_txid"].is_string () &&
        j.contains ("job_vout") && j["job_vout"].is_number_unsigned ())) return {};

    Bitcoin::txid txid {string {"0x"} + string (j["job_txid"])};
    if (!txid.valid ()) return {};

    return Bitcoin::outpoint {txid, uint32 (j["job_vout"])};
}

void pow_co::connect (
        net::asio::error_handler error_handler,
        net::close_handler closed,
        function<ptr<websockets_protocol_handlers> (ptr<net::session<const JSON &>>)> interact) {

    net::open_JSON_session ([] (const JSON::exception &err) -> void {
            throw err;
        }, [
            url = net::URL {net::protocol::WS, "5201", this->REST.Host, "/"},
            &io = this->IO, ssl = this->SSL, error_handler
        ] (net::close_handler closed, net::interaction<string_view, const string &> interact) -> void {
            net::websocket::open (io, url, ssl.get(), error_handler, closed, interact);
        }, closed, [interact] (ptr<net::session<const JSON &>> sx) -> handler<const JSON &> {
            std::cout << "zoob zoob zoob zub" << std::endl;
            return [handlers = interact (sx)] (const JSON &j) {
                std::cout << "websockets message received " << j << std::endl;
                if (!websockets_protocol_message::valid (j)) std::cout << "invalid websockets message received: " << j << std::endl;
                else if (j["type"] == "boostpow.job.created") {
                    auto prev = websockets_protocol_message::job_created (j["content"]);
                    if (prev) handlers->job_created (*prev);
                    else std::cout << "could not read websockets message " << j["content"] << std::endl;
                } else std::cout << "unknown message received: " << j << std::endl;
            };
        }
    );
}

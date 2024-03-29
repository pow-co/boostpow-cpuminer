#include <pow_co_api.hpp>
#include <data/net/websocket.hpp>
#include <data/net/JSON.hpp>

#include <boost/algorithm/hex.hpp>

string pow_co::write (const Bitcoin::txid &txid) {
    std::stringstream output_stream;
    output_stream << txid;
    return output_stream.str ().substr (9, 64);
}

string pow_co::write (const Bitcoin::outpoint &o) {
    std::stringstream output_stream;
    write (output_stream, o);
    return output_stream.str ();
}

Bitcoin::prevout read_job (const JSON &job,
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
    
    return Bitcoin::prevout {
        Bitcoin::outpoint {txid, index},
        Bitcoin::output {Bitcoin::satoshi {value}, *script_bytes}};
}

list<Bitcoin::prevout> pow_co::get_jobs_query::operator () () {

    std::cout << "getting ";
    if (bool (Limit)) std::cout << *Limit;
    else std::cout << "unlimited";
    std::cout << " jobs";
    if (bool (MaxDifficulty)) std::cout << " with max difficulty " << *MaxDifficulty;
    std::cout << "." << std::endl;

    list<entry<UTF8, UTF8>> params {};

    if (bool (Limit)) params <<= entry<UTF8, UTF8> {"limit", std::to_string (*Limit)};

    if (bool (Content)) params <<= entry<UTF8, UTF8> {"content", write (*Content)};

    if (bool (Tag)) params <<= entry<UTF8, UTF8> {"tag", *Tag};

    if (bool (MaxDifficulty)) params <<= entry<UTF8, UTF8> {"maxDifficulty", std::to_string (*MaxDifficulty)};

    if (bool (MaxDifficulty)) params <<= entry<UTF8, UTF8> {"minDifficulty", std::to_string (*MinDifficulty)};

    auto request = data::empty (params) ?
        PowCo.REST.GET ("/api/v1/boost/jobs") :
        PowCo.REST.GET ("/api/v1/boost/jobs", params);

    std::cout << "making request to " << request.URL << std::endl;

    auto response = PowCo (request);
    
    if (response.Status != net::HTTP::status::ok) {
        std::stringstream ss;
        ss << "response status is " << response.Status;
        throw net::HTTP::exception {request, response, ss.str () };
    }
    /*
    if (response.Headers[net::HTTP::header::content_type] != "application/JSON")
        throw net::HTTP::exception {request, response, "expected content type application/JSON"};
    */
    
    list<Bitcoin::prevout> boost_jobs;
    try {
        JSON JSON_jobs = JSON::parse (response.Body).at ("jobs");

        std::cout << "returned " << JSON_jobs.size () << " jobs ..." << std::endl;
        
        for (const JSON &job : JSON_jobs) boost_jobs = boost_jobs << read_job (job, request, response);
    } catch (const JSON::exception &j) {
        throw net::HTTP::exception {request, response, string {"invalid JSON format: "} + string {j.what ()}};
    }
    
    return boost_jobs;
}

inpoint pow_co::spends (const Bitcoin::outpoint &outpoint) {
    
    std::stringstream path_stream;
    path_stream << "/api/v1/spends/" << write (outpoint.Digest) << "/" << outpoint.Index;
    
    auto request = this->REST.GET (path_stream.str ());
    auto response = this->operator () (request);
    
    if (response.Status != net::HTTP::status::ok) {
        std::stringstream ss;
        ss << "response status is " << response.Status;
        throw net::HTTP::exception {request, response, ss.str () };
    }
    
    list<Bitcoin::prevout> boost_jobs;

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
    std::cout << "About to submit proof: " << request.URL << "\n\t" << request.Body << std::endl;
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

Bitcoin::prevout pow_co::job (const Bitcoin::txid &txid) {
    std::stringstream hash_stream;
    hash_stream << txid;
    
    std::stringstream path_stream;
    write (path_stream << "/api/v1/boost/jobs/", txid);
    
    auto request = this->REST.GET (path_stream.str ());
    
    auto response = (*this) (request);
    
    if (static_cast<unsigned int> (response.Status) >= 500)
        throw net::HTTP::exception{request, response, string {"problem reading txid."}};
    
    if (static_cast<unsigned int> (response.Status) != 200) {
        std::cout << "pow co returns response code " << response.Status << std::endl;
        std::cout << " response body " << response.Body << std::endl;

        throw net::HTTP::exception {request, response, string {"HTTP error response."}};
    }
    
    return read_job (JSON::parse (response.Body)["job"], request, response);
}

Bitcoin::prevout pow_co::job (const Bitcoin::outpoint &o) {
    std::stringstream hash_stream;
    hash_stream << o.Digest;
    
    std::stringstream path_stream;
    write (path_stream << "/api/v1/boost/jobs/", o);
    std::cout << "about to call for job " << path_stream.str ();
    auto request = this->REST.GET (path_stream.str ());
    
    auto response = (*this) (request);
    
    if (static_cast<unsigned int> (response.Status) >= 500)
        throw net::HTTP::exception {request, response, string {"problem reading txid."}};
    
    if (static_cast<unsigned int> (response.Status) != 200) {
        std::cout << "pow co returns response code " << response.Status << std::endl;
        std::cout << " response body " << response.Body << std::endl;

        throw net::HTTP::exception {request, response, string {"HTTP error response."}};
    }

    return read_job (JSON::parse (response.Body)["job"], request, response);
}

JSON pow_co::get_work_query::operator () () {
    std::cout << " calling get work " << std::endl;
    list<entry<UTF8, UTF8>> params {};

    if (bool (Limit)) params <<= entry<UTF8, UTF8> {"limit", std::to_string (*Limit)};

    if (bool (Tag)) params <<= entry<UTF8, UTF8> {"tag", *Tag};

    if (bool (Offset)) params <<= entry<UTF8, UTF8> {"offset", std::to_string (*Offset)};

    if (bool (Start)) params <<= entry<UTF8, UTF8> {"start", std::to_string (*Start)};

    if (bool (End)) params <<= entry<UTF8, UTF8> {"end", std::to_string (*End)};

    auto request = data::empty (params) ?
        PowCo.REST.GET ("/api/v1/boost/work") :
        PowCo.REST.GET ("/api/v1/boost/work", params);

    std::cout << " about to make request " << request.URL << std::endl;
    auto response = PowCo (request);

    if (static_cast<unsigned int> (response.Status) >= 500)
        throw net::HTTP::exception {request, response, string {"problem reading txid."}};

    if (static_cast<unsigned int> (response.Status) != 200) {
        std::cout << "pow co returns response code " << response.Status << std::endl;
        std::cout << " response body " << response.Body << std::endl;

        throw net::HTTP::exception {request, response, string {"HTTP error response."}};
    }

    try {

        return JSON::parse (response.Body);
        /*
        list<JSON> proofs;

        for (const JSON &j : work["work"]) proofs <<= j;

        return proofs;

            pow_co::work w;

            w.signature = j["signature"];

            w.value = uint64 (j["value"]);
            w.profitability = double (j["profitability"]);
            w.difficulty = double (j["difficulty"]);
            w.id = string (j["id"]);
            w.timestamp = string (j["timestamp"]);
            w.createdAt = string (j["createdAt"]);
            w.updatedAt = string (j["updatedAt"]);

            auto spend_txid_from_hex = encoding::hex::read (string (j["spend_txid"]));
            auto job_txid_from_hex = encoding::hex::read (string (j["job_txid"]));
            auto content_from_hex = encoding::hex::read (string (j["content"]));
            auto tag_from_hex = encoding::hex::read (string (j["tag"]));
            auto tx_from_hex = encoding::hex::read (string (j["tx_hex"]));

            if (!(bool (spend_txid_from_hex) &&
                bool (job_txid_from_hex) &&
                bool (content_from_hex) &&
                bool (tag_from_hex) &&
                bool (tx_from_hex))) throw net::HTTP::exception {request, response, string {"invalid hex format"}};

            std::copy (spend_txid_from_hex->begin (), spend_txid_from_hex->end (), w.spent.Digest.begin ());
            std::copy (job_txid_from_hex->begin (), job_txid_from_hex->end (), w.job.Digest.begin ());

            w.spent.Index = uint32 (j["spend_vout"]);
            w.job.Index = uint32 (j["job_vout"]);

            std::copy (content_from_hex->begin (), content_from_hex->end (), w.content.begin ());

            w.tag = *tag_from_hex;
            w.tx = *tx_from_hex;

            w.minerPubKey = Bitcoin::pubkey {string (j["minerPubKey"])};

            proofs <<= w;
        }*/

    } catch (const JSON::exception &ex) {
        throw net::HTTP::exception {request, response, string {"invalid JSON: "} + ex.what ()};
    }
}

bool pow_co::websockets_protocol_message::valid (const JSON &j) {
    return j.is_object () &&
        j.contains ("type") && j["type"].is_string () &&
        j.contains ("content");
}

std::optional<Bitcoin::prevout> pow_co::websockets_protocol_message::job_created (const JSON &j) {
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

    return Bitcoin::prevout {
        Bitcoin::outpoint {txid, uint32 (j["vout"])},
        Bitcoin::output {int64 (j["value"]), *hex_decoded}};
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
            url = net::URL (net::URL::make {}.protocol ("ws").port (5201).domain_name (this->REST.Host).path ("/")),
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

#ifndef BOOSTMINER_POW_CO_API
#define BOOSTMINER_POW_CO_API

#include <data/net/asio/session.hpp>
#include <data/net/HTTP_client.hpp>
#include <gigamonkey/boost/boost.hpp>

using namespace Gigamonkey;

struct inpoint : Bitcoin::outpoint {
    using Bitcoin::outpoint::outpoint;

    bool valid () const {
        return this->Digest.valid ();
    }

    inpoint (const Bitcoin::txid &t, uint32 i) : outpoint {t, i} {}
};

struct pow_co : net::HTTP::client_blocking {

    net::asio::io_context &IO;
    ptr<net::HTTP::SSL> SSL;
    
    pow_co (net::asio::io_context &io, ptr<net::HTTP::SSL> ssl, string host = "pow.co") :
        net::HTTP::client_blocking {ssl, net::HTTP::REST {"https", host}, tools::rate_limiter {3, 1}}, IO {io}, SSL {ssl} {}
    
    list<Bitcoin::prevout> jobs (uint32 limit = 10, double max_difficulty = -1);
    
    Bitcoin::prevout job (const Bitcoin::txid &);
    Bitcoin::prevout job (const Bitcoin::outpoint &);
    
    inpoint spends (const Bitcoin::outpoint &);
    
    void submit_proof (const bytes &);
    
    bool broadcast (const bytes &);

    struct websockets_protocol_message {
        string Type;
        JSON Content;

        bool valid () const;

        operator JSON () const;
        websockets_protocol_message (const JSON &);
        websockets_protocol_message ();

        static JSON encode (string type, const JSON &content);
        static bool valid (const JSON &);

        static std::optional<Bitcoin::prevout> job_created (const JSON &);
        static std::optional<Bitcoin::outpoint> proof_created (const JSON &);
    };

    struct websockets_protocol_handlers {
        virtual void job_created (const Bitcoin::prevout &) = 0;
        virtual ~websockets_protocol_handlers () {}
    };

    void connect (net::asio::error_handler error_handler, net::close_handler,
        function<ptr<websockets_protocol_handlers> (ptr<net::session<const JSON &>>)>);

    static string write (const Bitcoin::txid &);
    static string write (const Bitcoin::outpoint &);

    static std::ostream &write (std::ostream &, const Bitcoin::txid &);
    static std::ostream &write (std::ostream &, const Bitcoin::outpoint &);

    //void connect (net::asio::error_handler error_handler, net::close_handler, net::interaction<const JSON &>);
    
};

std::ostream inline &pow_co::write (std::ostream &o, const Bitcoin::txid &txid) {
    return o << write (txid);
}

std::ostream inline &pow_co::write (std::ostream &o, const Bitcoin::outpoint &out) {
    return o << write (out.Digest) << "_o" << out.Index;
}

#endif

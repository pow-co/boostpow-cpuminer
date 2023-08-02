
#include <argh.h>
#include <network.hpp>
#include <data/tools.hpp>
#include <data/net/JSON.hpp>
#include <data/io/wait_for_enter.hpp>
#include <gigamonkey/boost/boost.hpp>

using namespace Gigamonkey;

struct options {
    string APIHost {};
    string FileName {};
    options (const argh::parser &);
};

using error = maybe<std::string>;

struct proofs {
    map<digest256, bytes> Transaction;

    map<Bitcoin::outpoint, inpoint> Proofs;

    list<Bitcoin::outpoint> InvalidJobs;

    map<Bitcoin::outpoint, bytes> InvalidSpends;

    proofs () : Transaction {}, Proofs {} {}
    proofs (const JSON &j);

    operator JSON () const;

    static proofs load (const string &filename);
    void save (const string &filename);

    static maybe<Bitcoin::txid> read_txid (const std::string &);
    static maybe<inpoint> read_inpoint (const std::string &);
    static maybe<Bitcoin::outpoint> read_outpoint (const std::string &);

    static string write (const Bitcoin::txid &);
    static string write (const Bitcoin::outpoint &);
    static string write (const inpoint &);
};

struct solved {
    string id;
    Bitcoin::satoshi value;
    double profitability;
    double difficulty;
    Bitcoin::outpoint spent;
    Bitcoin::outpoint job;
    JSON signature;
    digest256 content;
    string timestamp;
    bytes tag;
    Bitcoin::pubkey minerPubKey;
    bytes tx;
    string createdAt;
    string updatedAt;

    solved (const JSON &);
    bool check_parameters ();

    solved () {}
};

error check_api (const options &o) {
    try {
        // search for file
        std::cout << "checking for file " << o.FileName << std::endl;
        proofs Proofs = proofs::load (o.FileName);
        wait_for_enter ();
        // call API
        std::cout << "calling API at " << o.APIHost << std::endl;

        BoostPOW::network Net = BoostPOW::network {o.APIHost};

        JSON Work = Net.PowCo.get_work () ()["work"];

        std::cout << "returned " << Work << std::endl;
        std::cout << Work.size () << " entries returned... " << std::endl;

        for (const JSON &j : Work) {
            wait_for_enter ();
            solved w {j};
            std::cout << " checking job " << w.job << std::endl;

            // check if the given parameters are correct.
            if (!w.check_parameters ()) {
                std::cout << " work " << j << " is incorrect." << std::endl;
                Proofs.InvalidJobs <<= w.job;
                continue;
            }

            // check against whatsonchain if the tx is correct.
            if (!Proofs.Transaction.contains (w.job.Digest)) {
                auto on_chain_job_tx = Net.get_transaction (w.job.Digest);

                // transaction has not been broadcast--we can do that ourselves.
                if (on_chain_job_tx.size () == 0) {
                    std::cout << " need to broadcast tx " << w.job.Digest << " ourselves." << std::endl;
                    Net.broadcast (w.tx);
                }

                Proofs.Transaction = Proofs.Transaction.insert (w.job.Digest, on_chain_job_tx);
            }

            // check against whatsonchain if the redeem txid is correct.
            if (!Proofs.Transaction.contains (w.spent.Digest)) {
                auto on_chain_spent_tx = Net.get_transaction (w.spent.Digest);

                if (on_chain_spent_tx.size () == 0) {
                    std::cout << " transaction can't be found onchain." << std::endl;

                    auto script_hash = SHA2_256 (Bitcoin::output::script (Bitcoin::transaction::output (w.tx, w.job.Index)));

                    bytes redeeming_tx;
                    for (const Bitcoin::txid &history_txid : Net.WhatsOnChain.script ().get_history (script_hash)) {
                        if (history_txid == w.job.Digest) continue;

                        auto history_tx = Net.get_transaction (history_txid);
                        Bitcoin::transaction htx {history_tx};

                        for (const auto &in : htx.Inputs) {
                            if (in.Reference == w.job) {
                                redeeming_tx = history_tx;
                                goto out;
                            }
                        }

                    }

                    out:

                    Proofs.InvalidSpends = Proofs.InvalidSpends.insert (w.job, redeeming_tx);
                } else Proofs.Transaction = Proofs.Transaction.insert (w.spent.Digest, on_chain_spent_tx);
            }

        }
        wait_for_enter ();
        std::cout << "saving file" << std::endl;
        Proofs.save (o.FileName);

        return error {};
    } catch (const net::HTTP::exception &e) {
        std::cout << "http error caught " << e.what () << std::endl;
        return error {std::string {"Http error: "} + e.what ()};
    } catch (const std::exception &e) {
        return error {std::string {"Error: "} + e.what ()};
    } catch (...) {
        return error {"Enknown error"};
    }
}

int main (int arg_count, char **arg_values) {
    auto err = check_api (options {argh::parser {arg_count, arg_values}});

    if (err) {
        std::cout << *err << std::endl;
        return 1;
    }

    return 0;
}

bool check_float (double x, double y, double range) {
    auto a = x / y - 1;
    auto b = y / x - 1;
    return a * a < range && b * b < range;
}

bool solved::check_parameters () {
    Bitcoin::transaction btx {tx};

    if (!btx.valid ()) {
        std::cout << " transaction is not valid" << std::endl;
        return false;
    }

    // does the given transaction have the given txid?
    auto txid = btx.id ();
    if (txid != job.Digest) {
        std::cout << " txid is not correct; expected " << job.Digest << " got " << txid << std::endl;
        return false;
    }

    // is there a boost script in the given transaction at the given index?
    auto boost_output = btx.Outputs[job.Index];
    Boost::output_script boost_script {boost_output.Script};
    if (!boost_script.valid ()) {
        std::cout << " boost script is not valid" << std::endl;
        return false;
    }

    // Does the script have the given parameters?
    if (boost_output.Value != value) {
        std::cout << " value is incorrect" << std::endl;
        return false;
    }

    if (boost_script.Content != content) {
        std::cout << " content is incorrect" << std::endl;
        return false;
    }

    if (boost_script.Tag != tag) {
        std::cout << " tag is incorrect" << std::endl;
        return false;
    }

    auto d = boost_script.Target.difficulty ();
    if (!check_float (d, difficulty, .001)) {
        std::cout << " difficulty is incorrect" << std::endl;
        return false;
    }

    auto p = double (boost_output.Value) / d;
    if (!check_float (p, profitability, .001)) {
        std::cout << " profitability is incorrect" << std::endl;
        return false;
    }

    return true;
}

options::options (const argh::parser &p) {
    p ("api_endpoint", "pow.co") >> APIHost;
    p ("filename", "check_api.json") >> FileName;
}

proofs::proofs (const JSON &j) : proofs {} {
    std::cout << "reading data " << j << std::endl;
    try {
        proofs p {};

        for (const std::pair<std::string, JSON> &e : j["transactions"]) {
            maybe<Bitcoin::txid> id = read_txid (e.first);
            if (!id) return;
            maybe<bytes> tx = encoding::hex::read (std::string (e.second));
            if (!tx) return;
            p.Transaction = p.Transaction.insert (*id, *tx);
        }

        for (const std::pair<std::string, JSON> &e : j["proofs"]) {
            maybe<Bitcoin::outpoint> out = read_outpoint (e.first);
            if (!out) return;
            maybe<inpoint> in = read_inpoint (std::string (e.second));
            if (!in) return;
            p.Proofs = p.Proofs.insert (*out, *in);
        }

        for (const JSON &b : j["invalid_jobs"]) {
            maybe<Bitcoin::outpoint> job = read_outpoint (b);
            if (!job) return;
            p.InvalidJobs <<= *job;
        }

        for (const std::pair<std::string, JSON> &e : j["invalid_pends"]) {
            maybe<Bitcoin::outpoint> out = read_outpoint (e.first);
            if (!out) return;
            maybe<bytes> in = encoding::hex::read (std::string (e.second));
            if (!in) return;
            p.InvalidSpends = p.InvalidSpends.insert (*out, *in);
        }

        std::cout << "read data as " << JSON (p) << std::endl;
        *this = p;
    } catch (const JSON::exception &) {}
}

proofs::operator JSON () const {
    JSON txs = JSON::object_t {};
    for (const entry<Bitcoin::txid, bytes> &e : Transaction)
        txs[write (e.Key)] = encoding::hex::write (e.Value);

    JSON proofs = JSON::object_t {};
    for (const entry<Bitcoin::outpoint, inpoint> &e : Proofs)
        proofs[write (e.Key)] = write (e.Value);

    JSON invalid_jobs = JSON::array_t {};
    for (const Bitcoin::outpoint &o : InvalidJobs)
        invalid_jobs.push_back (write (o));

    JSON invalid_spends = JSON::object_t {};
    for (const entry<Bitcoin::outpoint, bytes> &e : InvalidSpends)
        invalid_jobs[write (e.Key)] = encoding::hex::write (e.Value);

    JSON j = JSON::object_t {};
    j["txs"] = txs;
    j["proofs"] = proofs;
    j["invalid_jobs"] = invalid_jobs;
    j["invalid_spends"] = invalid_spends;

    return j;
}

solved::solved (const JSON &j) : solved {} {
    try {

        solved w;

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
            bool (tx_from_hex))) return;

        std::copy (spend_txid_from_hex->begin (), spend_txid_from_hex->end (), w.spent.Digest.begin ());
        std::copy (job_txid_from_hex->begin (), job_txid_from_hex->end (), w.job.Digest.begin ());

        w.spent.Index = uint32 (j["spend_vout"]);
        w.job.Index = uint32 (j["job_vout"]);

        std::copy (content_from_hex->begin (), content_from_hex->end (), w.content.begin ());

        w.tag = *tag_from_hex;
        w.tx = *tx_from_hex;

        auto pkstr = string (j["minerPubKey"]);
        w.minerPubKey = Bitcoin::pubkey {pkstr};

        *this = w;
    } catch (JSON::exception &) {}
}

maybe<Bitcoin::txid> proofs::read_txid (const std::string &x) {
    return Bitcoin::txid {std::string {"0x"} + x};
}

maybe<Bitcoin::outpoint> proofs::read_outpoint (const std::string &x) {
    auto txid = read_txid (x.substr (0, 64));
    std::stringstream ss {x.substr (65)};
    uint32 index;
    ss >> index;
    return Bitcoin::outpoint {*txid, index};
}

maybe<inpoint> proofs::read_inpoint (const std::string &x) {
    auto txid = read_txid (x.substr (0, 64));
    std::stringstream ss {x.substr (65)};
    uint32 index;
    ss >> index;
    return inpoint {*txid, index};
}

string proofs::write (const Bitcoin::txid &txid) {
    std::stringstream txid_stream;
    txid_stream << txid;
    string txid_string = txid_stream.str ();
    if (txid_string.size () < 73) throw std::logic_error {std::string {"warning: txid string was "} + txid_string};
    return txid_string.substr (7, 66);
}

string proofs::write (const Bitcoin::outpoint &o) {
    std::stringstream ss;
    ss << write (o.Digest) << ":" << o.Index;
    return ss.str ();
}

string proofs::write (const inpoint &i) {
    std::stringstream ss;
    ss << write (i.Digest) << ":" << i.Index;
    return ss.str ();
}

#include <fstream>

proofs proofs::load (const string &filename) {
    std::fstream file;
    file.open (filename, std::ios::in);
    if (!file) return proofs {};
    return proofs {JSON::parse (file)};
}

void proofs::save (const string &filename) {
    std::fstream file;
    file.open (filename, std::ios::out);
    if (!file) throw exception {"could not open file"};
    file << JSON (*this).dump ();
    file.close ();
}

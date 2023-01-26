#include <miner_options.hpp>
#include <argh.h>
#include <gigamonkey/script/typed_data_bip_276.hpp>
#include <gigamonkey/schema/hd.hpp>

namespace BoostPOW {

    int run_spend (const argh::parser &command_line, int (*spend) (const script_options &)) {

        script_options options;

        string content_hex;

        if (auto positional = command_line (2); positional) positional >> content_hex;
        else if (auto option = command_line ("content"); option) option >> content_hex;
        else throw data::exception {"option content not provided "};

        options.Content = digest256 {content_hex};
        if (!options.Content.valid ()) throw data::exception {} << "invalid content string: " << content_hex;

        if (auto positional = command_line (3); positional) positional >> options.Difficulty;
        else if (auto option = command_line ("difficulty"); option) option >> options.Difficulty;
        else throw data::exception {"option difficulty not provided "};

        if (options.Difficulty <= 0) throw data::exception {} << "difficulty must be >= 0; value provided was " << options.Difficulty;

        if (auto positional = command_line (4); positional) options.Topic = positional.str ();
        else if (auto option = command_line ("topic"); option) options.Topic = option.str ();

        if (auto positional = command_line (5); positional) options.Data = positional.str ();
        else if (auto option = command_line ("data"); option) options.Data = option.str ();

        string address;

        if (auto positional = command_line (6); positional) positional >> address;
        else if (auto option = command_line ("address"); option) option >> address;

        if (address != "") {
            options.MinerAddress = Bitcoin::address {address};
            if (!options.MinerAddress->valid ()) throw data::exception {} << "invalid address provided: " << address;
        }

        if (auto option = command_line ("version"); option) option >> options.Version;

        if (options.Version < 1 || options.Version > 2) throw data::exception {} << "invalid script version " << options.Version;

        return spend (options);

    }

    mining_options read_mining_options (const argh::parser &command_line, int arg_position) {

        mining_options options;

        string secret_string;
        if (auto positional = command_line (arg_position); positional) positional >> secret_string;
        else if (auto option_wif = command_line ("wif"); option_wif) option_wif >> secret_string;
        else if (auto option_secret = command_line ("secret"); option_secret) option_secret >> secret_string;
        else throw data::exception {"secret key not provided"};

        ptr<key_source> signing_keys;
        ptr<address_source> receiving_addresses;

        Bitcoin::secret key {secret_string};
        HD::BIP_32::secret hd_key {secret_string};

        if (key.valid ()) options.SigningKeys =
            std::static_pointer_cast<key_source> (std::make_shared<single_key_source> (key));
        else if (hd_key.valid ()) options.SigningKeys =
            std::static_pointer_cast<key_source> (std::make_shared<HD::key_source> (hd_key));
        else throw data::exception {"could not read signing key"};

        string address_string;
        if (auto positional = command_line (arg_position + 1); positional) positional >> address_string;
        else if (auto option = command_line ("address"); option) option >> address_string;

        if (address_string == "") {
            if (key.valid ()) options.ReceivingAddresses =
                std::static_pointer_cast<address_source> (std::make_shared<single_address_source> (key.address ()));
            else if (hd_key.valid ()) options.ReceivingAddresses =
                std::static_pointer_cast<address_source> (std::make_shared<HD::address_source> (hd_key.to_public ()));
            else throw data::exception {"could not read receiving address"};
        } else {
            Bitcoin::address address {address_string};
            HD::BIP_32::pubkey hd_pubkey {address_string};

            if (address.valid ()) options.ReceivingAddresses =
                std::static_pointer_cast<address_source> (std::make_shared<single_address_source> (address));
            else if (hd_pubkey.valid ()) options.ReceivingAddresses =
                std::static_pointer_cast<address_source> (std::make_shared<HD::address_source> (hd_pubkey));
            else throw data::exception {"could not read receiving address"};
        }

        if (auto option = command_line ("threads"); option) option >> options.Threads;
        if (options.Threads == 0) throw data::exception {"need at least one thread"};

        double fee_rate;
        if (auto option = command_line ("fee_rate"); option) {
            option >> fee_rate;
            options.FeeRate = fee_rate;
        }

        if (auto option = command_line ("api_endpoint"); option) options.APIHost = option.str ();

        return options;
    }

    int run_redeem (const argh::parser &command_line,
        int (*redeem) (const Bitcoin::outpoint &, const Boost::output_script &, int64_t, const mining_options &)) {

        Bitcoin::outpoint outpoint;

        string txid_string;
        if (auto positional = command_line (2); positional) positional >> txid_string;
        else if (auto option = command_line ("txid"); option) option >> txid_string;
        else throw data::exception {"option txid not provided "};

        outpoint.Digest = Bitcoin::txid {txid_string};
        if (!outpoint.Digest.valid ()) throw data::exception {} << "could not read txid " << txid_string;

        if (auto positional = command_line (3); positional) positional >> outpoint.Index;
        else if (auto option = command_line ("index"); option) option >> outpoint.Index;

        Boost::output_script boost_script {};

        {
            string script_string;
            if (auto positional = command_line (4); positional) positional >> script_string;
            else if (auto option = command_line ("script"); option) option >> script_string;
            else goto no_script_provided;

            bytes *script;
            ptr<bytes> script_from_hex = encoding::hex::read (script_string);
            typed_data script_from_bip_276 = typed_data::read (script_string);

            if (script_from_bip_276.valid ()) script = &script_from_bip_276.Data;
            else if (script_from_hex != nullptr) script = &*script_from_hex;
            else throw data::exception {"could not read script"};

            boost_script = Boost::output_script::read (*script);
        }

        no_script_provided:

        int64 satoshi_value = -1;
        if (auto positional = command_line (5); positional) positional >> satoshi_value;
        else if (auto option = command_line ("value"); option) option >> satoshi_value;

        return redeem (outpoint, boost_script, satoshi_value, read_mining_options (command_line, 6));

    }

    int run_mine (const argh::parser &command_line,
        int (*mine) (double min_profitability, double max_difficulty, const mining_options &)) {

        double min_profitability = 0, max_difficulty = 0;

        if (auto option = command_line ("min_profitability"); option) option >> min_profitability;
        if (auto option = command_line ("max_difficulty"); option) option >> max_difficulty;

        return mine (min_profitability, max_difficulty, read_mining_options (command_line, 2));
    }

    int run (const argh::parser &command_line,
        int (*help) (),
        int (*version) (),
        int (*spend) (const script_options &),
        int (*redeem) (const Bitcoin::outpoint &, const Boost::output_script &, int64, const mining_options &),
        int (*mine) (double min_profitability, double max_difficulty, const mining_options &)) {

        try {
            if (command_line["version"]) return version ();

            if (command_line["help"]) return help ();

            if (!command_line (1)) throw data::exception {"Must provide a command (help, version, spend, redeem, mine)"};

            string method = command_line (1).str ();

            if (method == "help") return help ();

            if (method == "version") return version ();

            if (method == "spend") return run_spend (command_line, spend);

            if (method == "redeem") return run_redeem (command_line, redeem);

            if (method == "mine") return run_mine (command_line, mine);

            throw data::exception {} << "Invalid method " << method << " called. Must be help, version, spend, redeem, or mine";

        } catch (const std::string x) {
            std::cout << "Error: " << x << std::endl;
            return 1;
        } catch (const data::exception &x) {
            std::cout << "Error: " << x.what() << std::endl;
            return 1;
        } catch (const std::exception &x) {
            std::cout << "Unexpected error: " << x.what() << std::endl;
            return 1;
        } catch (...) {
            std::cout << "Unknown error " << std::endl;
            return 1;
        }

        return 1;
    }

}

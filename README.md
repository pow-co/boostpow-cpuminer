# CPU Miner for Boost

This c++ program depends on the Gigamonkey repository and is designed to pull in all its dependencies via conan. It shall also be pulished as a conan package so that other c++ programs can easily pull it in as a dependency where needed.

For distribution it is meant that this repository may be built for mac, linux, and windows, as well as packaged for Docker. Ideally also there will be an apt package repository.

## Run from docker

This is the easiest way to use BoostMiner. 

1. Download the latest master branch of this software. 
2. `docker build -t boostpow .` (This should take a while.)
3. `docker run boostpow ./BoostMiner help` This will show the help message for the mining software.
   See heading *Usage* below. 

## Installation

We use conan package manager to install the dependencies and build the applicatoin
```bash
conan config set general.revisions_enabled=True

conan remote add proofofwork https://conan.pow.co/artifactory/api/conan/conan
```
Is needed to add the correct repository to conan.

`conan install . -r=proofofwork` will install gigamonkey and its dependencies from artifactory

`conan build .` will output the BoostMiner program compiled for your platform

## Usage

```
./BoostMiner help
input should be <function> <args>... --<option>=<value>... where function is 
	spend      -- create a Boost output.
	redeem     -- mine and redeem an existing boost output.
	mine       -- call the pow.co API to get jobs to mine.
For function "spend", remaining inputs should be 
	content    -- hex for correct order, hexidecimal for reversed.
	difficulty -- a positive number.
	topic      -- (optional) string max 20 bytes.
	add. data  -- (optional) string, any size.
	address    -- (optional) If provided, a boost contract output will be created. Otherwise it will be boost bounty.
For function "redeem", remaining inputs should be 
	script     -- boost output script, hex or bip 276.
	value      -- value in satoshis of the output.
	txid       -- txid of the tx that contains this output.
	index      -- index of the output within that tx.
	wif        -- private key that will be used to redeem this output.
	address    -- (optional) your address where you will put the redeemed sats.
	              If not provided, addresses will be generated from the key. 
For function "mine", remaining inputs should be 
	key        -- WIF or HD private key that will be used to redeem outputs.
	address    -- (optional) your address where you will put the redeemed sats.
	              If not provided, addresses will be generated from the key. 
options for functions "redeem" and "mine" are 
	threads           -- Number of threads to mine with. Default is 1.
	min_profitability -- Boost jobs with less than this sats/difficulty will be ignored.
	max_difficulty    -- Boost jobs above this difficulty will be ignored.
	fee_rate          -- sats per byte of the final transaction.
	                     If not provided we get a fee quote from Gorilla Pool.
```




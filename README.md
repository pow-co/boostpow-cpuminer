# CPU Miner for Boost

This c++ program depends on the Gigamonkey repository and is designed to pull in all its dependencies via conan. It shall also be pulished as a conan package so that other c++ programs can easily pull it in as a dependency where needed.

For distribution it is meant that this repository may be built for mac, linux, and windows, as well as packaged for Docker. Ideally also there will be an apt package repository.

## Installation

We use conan package manager to install the dependencies and build the applicatoin

`conan install .` will install gigamonkey and its dependencies from artifactory

`conan build .` will output the BoostMiner program compiled for your platform

## Usage

```
./BoostMiner help
input should be "function" "args"... where function is 
	spend      -- create a Boost output.
	redeem     -- mine and redeem an existing boost output.
For function "spend", remaining inputs should be 
	content    -- hex for correct order, hexidecimal for reversed.
	difficulty -- 
	topic      -- string max 20 bytes.
	add. data  -- string, any size.
	address    -- OPTIONAL. If provided, a boost contract output will be created. Otherwise it will be boost bounty.
For function "redeem", remaining inputs should be 
	script     -- boost output script.
	value      -- value in satoshis of the output.
	txid       -- txid of the tx that contains this output.
	index      -- index of the output within that tx.
	wif        -- private key that will be used to redeem this output.
	address    -- your address where you will put the redeemed sats.
	
wallet file format is json: 

{
  'prevouts': [
    {
      'txid': <hex string>       // 
      'index': <number>          // 
      'value': <number>          //
      'script': <hex string>     //
    }, 
    ...                          // any number of these.
  ], 
  'key': <hd priv key string>    //
  'index': <number>              //
}
```




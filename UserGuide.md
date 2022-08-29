# How to Mine Boost Jobs Using BoostMiner for Terminal

## 1. Get BoostMiner

In your terminal, type the following command and hit enter:

`docker run katgaea/boostminer:v1.1 ./bin/BoostMiner`

BoostMiner will install. You will be greeted with this text:

```
input should be "function" "args"... where function is 
	spend      -- create a Boost output.
	redeem     -- mine and redeem an existing boost output.
For function "spend", remaining inputs should be 
	content    -- hex for correct order, hexidecimal for reversed.
	difficulty -- 
	topic      -- string max 20 bytes. (must be in hex)
	add. data  -- string, any size.
	address    -- OPTIONAL. If provided, a boost contract output will be created. Otherwise it will be boost bounty.
For function "redeem", remaining inputs should be 
	script     -- boost output script.
	value      -- value in satoshis of the output.
	txid       -- txid of the tx that contains this output.
	index      -- index of the output within that tx.
	wif        -- private key that will be used to redeem this output.
	address    -- your address where you will put the redeemed sats.
```

## 2. Construct a Command

There are two types of commands that BoostMiner can understand. One is `spend` and the other is `redeem`. 

- `spend` is when you a creating a boost job. For this type of command, you need to put some amount of satoshis into a locked puzzle and dictate what level of difficulty will unlock the puzzle and reward the miner with the satoshis.

- `redeem` is when you want to earn satoshis that are currently already locked in a spend script. The satoshi reward waits for proof that sufficient energy has been wasted producing a solution that meets or exceeds the puzzle's required difficulty.

Each command has a number of parameters required. Let's give you an example of each type and then break them down into pieces for clarity.

### **Examples**

- `spend` command in BoostMiner:

`boostminer spend (content) (difficulty) (topic) (additional data) (address)`

- `redeem` command in BoostMiner:

`boostminer redeem (script) (value) (txid) (index) (wif) (address)`

---
## The `spend` Command

### **Use this to boost a new piece of content**

This is how a BoostMiner `spend` command is constructed:

`boostminer spend (content) (difficulty) (topic) (additional data) (address)`

Here is an example of a command with each of those parts entered to boost the BoostPOW Proof of Work White Paper by Daniel Krawisz:

`boostminer spend (content) (difficulty) (topic) (additional data) (address)`

### Let's break down each of those components:

### 1. `boostminer` 

This is the name of the program running in your command line. 
- Beginning the command with this word indicates to the computer what program to run.
- Whatever follows indicates to the program how to execute the data that follows.

### 2. `spend`

This is the command type that BoostMiner is running in this instance. 
- Some specific content follows this so that BoostMiner can construct a valid script.
- BoostMiner will use this data to construct a type of puzzle where the creator locks up satoshis and sets a level of difficulty that will unlock the puzzle and reward the unlocker with those satoshis.

### 3. `content`

The content portion of the command is where you enter the hash of the content that you want to boost. 
- This could be a hash of anything: pdf, mp3, mp4, txt, etc.
- If you are boosting a Bitcoin TXID, write `0x<txid>` 
- For anything else, provide the hash in standard hex.

### 4. `difficulty`

The difficulty is a number that represents the amount of energy a computer must exert in order to solve a particular puzzle.
- The level of difficulty is arbitrary and set by the creator of the puzzle.
- Miners expect higher rewards to coincide with solving higher difficulty puzzles.
- Therefore, the higher the difficulty you set, the more satoshis you want to lock up for the person who solves the puzzle.

### 5. `topic`

The topic portion of the command gives users an easy way to categorize content that they are boosting. 

- Topics are `strings` limited to 20 bytes in hex format. 
- Think of them like a hashtag or category. 
- Topics help narrow search results for people who are searching for content of a particular variety or subject matter.

Example: `46616d696c79`

### 6. `additional data`

The additional data portion of the `spend` command is where the person boosting the content can add any information about the file that they want.

- A `string` of information any size.
- Examples include: 
  - credits for the creator
  - contact information of the booster
  - cryptographic signature proving authenticity

### 7. `address` *(OPTIONAL)*

If the user DOES provide a valid Bitcoin address:
- BoostMiner uses that address to create a contract rewarding the locked satoshis to that address when the boost job is mined.

If the user DOES NOT provide a valid Bitcoin address:
- BoostMiner writes a contract that turns the locked satoshis into a bounty that can be claimed by the person or entity that successfully mines the boost job.

---

## The `redeem` Command

### **Use this to get rewarded with satoshis for mining existing boost jobs**

`boostminer redeem (script) (value) (0x<txid> | hash) (index) (wif) (address)`

### 1. `script`
**Boost output script**

When a person or entity runs the `spend` command, BoostMiner generates a Boost output `script`. 

- Provide the output script of the boost job that you want to mine.
- The script is a hex `string`.
- You can find the script using a block explorer such as https://WhatsOnChain.com and looking at the details of any transaction to find "ScriptHash".

Example script: 
```
08626f6f7374706f7775040000000020d0767187e33c54cf1289c916d43e8290138933adf023a5203bc88a2b3b6a69a904f8cf071e00043d4b5108007e7c557a766b7e52796b557a8254887e557a8258887e7c7eaa7c6b7e7e7c8254887e6c7e7c8254887eaa01007e816c825488537f7681530121a5696b768100a0691d00000000000000000000000000000000000000000000000000000000007e6c539458959901007e819f6976a96c88ac
```

### 2. `value`
**Number of satoshis of the output**

This is the amount of satoshis that are locked up in the script to be redeemed.

- This number is an integer
- Examples include:
  - `96000` satoshis = `0.0096000` Bitcoin
  - `123456789` satoshis = `1.23456789` Bitcoin

### 3. `txid`

**txid of the tx that contains this output**

BoostMiner provides users with a TXID when it constructs a boost job puzzle.

- This can be copy and pasted from anywhere that has a valid boost job available.
- The order can be backwards (as is standard with Bitcoin TXIDs) and DOES NOT need to be preceded by `0x`.
- Find an example of one of these at https://AskBitcoin.com by pressing the Boost button beside a question.

### 4. `index`

**index of the output within that tx**

Bitcoin transactions are comprised of a series of inputs and outputs. Those inputs and outputs can be identified by their `index`.
- Provide the index that contains the script to be boosted
- Sometimes this is indicated on a block explorer
- An integer, usually `0`, `1`, or `2` for standard bitcoin transactions

### 5. `wif`

**private key that will be used to redeem this output**

Generate a private key for the `redeem` command. You can create a new wallet in ElectrumSV or do it any number of ways. The important thing is that you get a valid private key. Some people even write random text and use hash256 to generate a valid private key.

- The private key is for signing the solved puzzle.
- `wif` DOES NOT need to correspond with the public address you provide.
  - (That address is where the reward goes. This private key is for signing that you are the one that solved the puzzle.)
- Example: `<private key redacted for security>`

### 6. `address`

**your address where you will put the redeemed sats**

- Standard Bitcoin Address
- Public Key Hash
- Example: `19Qe9VsEtTSJ7kUwMtQmd2HuBUMWeT9PRt`

## 3. Put It All Together

It can help to open up a text editor to put these pieces together individually.

**Want to Mine Boost?**

Here is an example of the command to run to mine a boost job. 
 - *(This boost job may already have been redeemed by the time you read this.)*
 - ```boostminer redeem 93f5e8a3e5bf2f8152733a68c5e271970b22efee55fbec4d29bea428bf786a32 96000 4288a7f32c47cb9932a1d4b3481c4fe052002e4279789af2209be546ac88e214 0 <private key redacted for security> 19Qe9VsEtTSJ7kUwMtQmd2HuBUMWeT9PRt```
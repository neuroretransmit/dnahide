# dnahide

Hide data in GenBank DNA sequence files using authenticated encryption.

## Info

* **Compression**: LZMA
* **Encryption**: RC6 in GCM-SIV using authenticated data or in CTR using ECB on individual blocks
* **TODO**: Add secret sharing scheme for exchange of authenticated data

## Usage

```
dnahide - hide messages in DNA
Options:
  -h [ --help ]         print help messages
  -u [ --unsteg ]       unsteg message
  -i [ --input ] arg    input file
  -o [ --output ] arg   output file
  -p [ --password ] arg encryption password
  -a [ --aad ] arg      additional authenticated data
```

### Stegging

```
$ ./dnahide --input some_file --password test --aad authentication_data
[*] File size: 625 bytes
[*] Compressing...
[*] Encrypting data...
[*] Encoding DNA...

LOCUS       GXP_8263708(PAX6/human) 1113 bp DNA  
ACCESSION   GXP_8263708
BASE COUNT  304 a 279 c 278 g 252 t 
ORIGIN
        1 TACCATCTTG GAGTCGGATT ATTAAAATGA AAAATAGCCG TTCCGGCATT GTGTGCCAAC 
       61 AAATAGAGCC GGCGAATTAT AGAGGTGAGT AGAGTGCAAG CACGCGGGTC CTGAAAGAAC 
      121 TGCGGGTGTT CCGTGCATAC ACCAATAATT TGTGTTCGCA GATACATACT TGACCCATAC 
      181 CGACAAGTGT GAATATTGAG ACTGCCATCC GATGTCGACA AAGATTGATG TCTGTGCCTG 
      241 GTCTTATCAA TCGCTACTCT AGGACATCCC ATACTCACCA TTACATAGCG GCCAAGACAG 
      301 TGCTCACGGA CCCATAGCAA CAAGAAGCTA GCATCATGCT GGACGCCTAT CGTGAACGCC 
      361 GGGCTGGGAT GCGTAACCAT CCGCACCGTG TATCCACCAT CTCCACGCAA GCGCAGGAGG 
      421 GCTATCCCCG CGACGGGTAC TAGATCTCAT GATATAAAAG GGACCGATGA TCCTTTGATA 
      481 CACTACTAGA ACAGATTACG ATCTCGGTTA TACTCCATGC ATCGTGCTCG ACCCACTAAT 
      541 TAGCACCCCA GCGTGCGACC TTTCCGACAA TGATGACGTG TGACATCGAC GCTGGAGGCT 
      601 GAGGTGTTGG CCAGGTCCTT TTTCAGAGCG TGCGGCCTCG GGAGGTGTCG AGGAAGGCGA 
      661 ACCCCATATA TTTCCATCTT TCTGTAGAGT CTGCTACCTG GCCTAGAAAT GATTTGCTAC 
      721 CAACGCCCAC CACCTGGCAG CCTAACTTGA ATAAGGAAGA CTTGAAATAG CATCAGCAGC 
      781 GATGGAATAG CGTTAACCTG GGGTTTTGCT AGTGCAGACC TGATCTGATG GTATGCGACC 
      841 AGATACTGGC CTGCGAGGAA CATAGGACTA ACCCGAACCT CGGACAAAGA GCAGTTAGAG 
      901 CTAGAACAGA GCCACAACTC GAAGCACCGA AGATCTGCGC ACCATACCAG CAACGGAGGT 
      961 GTTTTCACGG TGAAATCGCT CCAGAGTCCT TTTAGACTCA TGAAGGTAAT AGTCAGGCAA 
     1021 GCTAGGTTAG AGTTAACAGA GCTCGAGGTC CGTGTGGGTT ATCTGCATTG CACACGAACC 
     1081 AGATTCATTA CCGAGATAAT GGGCTACTTG CTC 
```

### Unstegging

```
$ ./dnahide --input test.gb --unsteg --password test --aad authentication_data 
[*] Decoding DNA...
[*] Decrypting data...
[*] Decompressing data...

<<< BEGIN RECOVERED MESSAGE >>>

[
{
  "directory": "/home/jonesy/git/dnahide/build/src",
  "command": "/usr/bin/c++  -DBOOST_ALL_NO_LIB -DBOOST_PROGRAM_OPTIONS_DYN_LINK   -Wall -Werror -Wextra -Os -g   -pthread -o CMakeFiles/dnahide.dir/main.cc.o -c /home/jonesy/git/dnahide/src/main.cc",
  "file": "/home/jonesy/git/dnahide/src/main.cc"
},
{
  "directory": "/home/jonesy/git/dnahide/build/src",
  "command": "/usr/bin/cc -DBOOST_ALL_NO_LIB -DBOOST_PROGRAM_OPTIONS_DYN_LINK  -g   -pthread -o CMakeFiles/dnahide.dir/crypto/fastbpkdf2.c.o   -c /home/jonesy/git/dnahide/src/crypto/fastbpkdf2.c",
  "file": "/home/jonesy/git/dnahide/src/crypto/fastbpkdf2.c"
}
]
<<< END RECOVERED MESSAGE >>>

```

### Shorthand

**Steg message without encryption**
```
./dnahide -i msg -o msg.gb
```

**Unsteg message without encryption**
```
./dnahide -i msg.gb -u -o msg.decoded
```

**Steg message with encryption**
```
./dnahide -i msg -p test -o msg.gb
```

**Unsteg message with encryption**
```
./dnahide -i msg.gb -u -p test -o msg.decoded
```

**Steg message with authenticated encryption** (where authentication.bin is a shared secret between both parties)
```
./dnahide -i msg -p test -a $(cat authentication.bin) -o msg.gb
```

**Unsteg message with authenticated encryption** (where authentication.bin is a shared secret between both parties)
```
./dnahide -i msg.gb -u -p test -a $(cat authentication.bin) -o msg.decoded
```

**Piped Input**
```
echo "test" | ./dnahide -p test -a $(cat authentication.bin) -o msg.decode
```

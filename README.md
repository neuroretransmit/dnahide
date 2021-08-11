# dnahide

Hide data in GenBank DNA sequence files with authenticated encryption.

Input data goes through the following process:

1. Compress with LZMA
2. Encrypt using RC6 in GCM-SIV using authenticated data (can be a public key, anything under 64GB)
    - TODO: Add unauthenticated version that uses RC6 in another mode.
3. Base64 encode using character-level mapping of base64 character->3-letter codon.
    - TODO: Enable base64 index shuffling
4. Create GenBank DNA sequence file format and overlay stegged DNA.
    - TODO: Finish randomly generating metadata/DESCRIPTION section

## Usage

```
dnahide - hide messages in DNA
Options:
  -h [ --help ]         print help messages
  -u [ --unsteg ]       unsteg message
  -i [ --input ] arg    Input file
  -o [ --output ] arg   output file
  -p [ --password ] arg encryption password
  -a [ --aad ] arg      additional authenticated data
```

### Stegging

```
$ ./dnahide --input some_file --password test -aad authentication_data
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

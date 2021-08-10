# dnahide

**In-progress! GenBank format and provided authenticated data unsupported yet**

Hide data in GenBank DNA sequence files with authenticated encryption.

Input data goes through the following process:

1. Compress with LZMA
2. Encrypt using RC6 in GCM-SIV mode using provided authentication data
    - TODO: Add authenticated data parameter, currently hardcoded
    - TODO: Drop in key derivation function instead of using sha256 of password
    - TODO: Add unauthenticated version that uses RC6 in another mode.
3. Base64 encode using character-level mapping of base64->3-letter codon.
    - TODO: Enable base64 index shuffling
4. Create GenBank DNA sequence file format and overlay stegged DNA.
    - TODO: Randomly generate metadata
    - TODO: Write formatter
    - TODO: Write lexer/parser


## Example Output

```
LOCUS       GXP_4405072(PAX6/human)    1105 bp    DNA
DEFINITION  loc=GXL_141121|sym=PAX6|geneid=5080|acc=GXP_4405072|
            taxid=9606|spec=Homo sapiens|chr=11|ctg=NC_000011|str=(-)|
            start=31806821|end=31807925|len=1105|tss=1001,1005|
            homgroup=-|promset=-|eldorado=E32R1605|descr=paired box 6|
            comm=GXT_25635656/ENST00000455099/1005/gold;
            GXT_27757207/NM_001310159/1001/bronze
ACCESSION   GXP_4405072
BASE COUNT    229 a  239 c  313 g  324 t
ORIGIN
        1 GACTTTTTTT TTTTTTCCTT TGGGAAAGGT AGGGAGGTGT TCGTACGGGA GCAGCCTCGG
       61 GGACCCCTGC ACTGGGTCAG GGCTTATGAA GCTAGAAGCG TCCCTCTGTT CCCTTTGTGA
      121 GTTGGTGGGT TGTTGGTACA TTTGGTTGGA AGCTGTGTTG CTGGTTAGGG AGACTCGGTT
      181 TTGCTCCTTG GGTTCGAGGA AAGCTGGAGA ATAGAAGCCA TTGTTTGCCG TCTGTCGGCT
      241 TTGTCGACCA CGCTCACCCC CTCCTGTTCG TACTTTTTAA AGCAGTGAGG CGAGGTAGAC
      301 AGGGTGTGTC ACAGTACAGT TAAAGGGGTG AAGATCTAAA CGCCAAAAGA GAAGTTAATC
      361 ACAATAAGTG AGGTTTGGGA TAAAAAGTTG GGCTTGCCCC TTTCAAAGTC CCAGAAAGCT
      421 GGGAGGTAGA TGGAGAGGGG GCCATTGGGA AGTTTTTTTG GTGTAGGGAG AGGAGTAGAA
      481 GATAAAGGGT AAGCAGAGTG TTGGGTTCTG GGGGTCTTGT GAAGTTCCTT AAGGAAGGAG
      541 GGAGTGTGGC CCTGCAGCCC TCCCAAACTG CTCCAGCCTA TGCTCTCCGG CACCAGGAAG
      601 TTCCAAGGTT CCCTTCCCCT GGTCTCCAAA CTTCAGGTAT TCCTCTCCCC TCACACCCCT
      661 TCAACCTCAG CTCTTGGCCT CTACTCCTTA CTCCACTGTT CCTCCTGTTT CCCCCTTCCC
      721 CTTTTCCTGG TTCTTTATAT TTTTGCAAAG TGGGATCCGA ACTTGCTAGA TTTTCCAATT
      781 CTCCCAAGCC AGACCAGAGC AGCCTCTTTT AAAGGATGGA GACTTCTGTG GCAGATGCCG
      841 CTGAAAATGT GGGTGTAATG CTGGGACTTA GAGTTTGATG ACAGTTTGAC TGAGCCCTAG
      901 ATGCATGTGT TTTTCCTGAG AGTGAGGCTC AGAGAGCCCA TGGACGTATG CTGTTGAACC
      961 ACAGCTTGAT ATACCTTTTT CTCCTTCTGT TTTGTCTTAG GGGGAAGACT TTAACTAGGG
     1021 GCGCGCAGAT GTGTGAGGCC TTTTATTGTG AGAGTGGACA GACATCCGAG ATTTCAGGCA
     1081 AGTTCTGTGG TGGCTGCTTT GGGCT
```

## Usage

```
DNAHide - Hide messages in DNA
Options:
  -h [ --help ]         Print help messages
  -u [ --unsteg ]       Unsteg message
  -i [ --input ] arg    Input file
  -o [ --output ] arg   Output file
  -p [ --password ] arg Encryption password
  -a [ --aad ] arg      Additional authenticated data
```

# dnahide

Hide data in GenBank DNA sequence files using authenticated encryption.

## Info

* **Compression**: LZMA
* **Encryption**: RC6 in GCM-SIV using authenticated data or in CTR using ECB on individual blocks

## Examples

Stegged examples can be found in [examples/](examples/).

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

Without encryption

```
./dnahide -i msg -o msg.gb
```

With encryption
```
./dnahide -i msg -p test -o msg.gb
```

With authenticated encryption (where authentication.bin is a shared secret between both parties)
```
./dnahide -i msg -p test -a $(cat authentication.bin) -o msg.gb
```
### Unstegging

Without encryption
```
./dnahide -i msg.gb -u -o msg.decoded
```

With encryption
```
./dnahide -i msg.gb -u -p test -o msg.decoded
```

With authenticated encryption (where authentication.bin is a shared secret between both parties)
```
./dnahide -i msg.gb -u -p test -a $(cat authentication.bin) -o msg.decoded
```

### Shorthand

Piped Input
```
cat msg | ./dnahide -u -p test -a $(cat authentication.bin) -o msg.decode
```

Redirection (headers and information on command line is printed to stderr so redirection only writes GenBank or unstegged file)
```
./dnahide -i -p test -a $(cat authentication.bin) > msg.decode
```

# parse the symboles.txt file 
# (outputed from nm -n)
# and create symbols.dat, following this format:
# struct {
#     uint64_t n_symbs;
#     struct S symboles[n_symbs];
# };
#  
# struct S {
#   uint64_t addr;
#   char     name[56];
#   // null-terminated string
# };
#  
#  
# 
import sys

f    = open("symbols.txt", "r")
outf = open("symbols.dat", "wb")

outf.write((0).to_bytes(8, 'big'))


n_symbols = 0

for line in f.readlines():
    words = line.split()
    if words[1] != 'T' and words[1] != 't':
        continue
    
    addr = int(words[0], 16)
    name = words[2]
    outf.write(addr.to_bytes(8, 'big'))

    bin = bytes(name, 'ascii')

    #print(name)
    if len(bin) >= 56:
        print("WARNING: symbole name " + name 
            + " is too long and will be truncated", file=sys.stderr)
        
        bin = bin[:55] +  b'\x00'

    else:
        bin = bin + (0).to_bytes(56 - len(bin), 'little')

    outf.write(bin)
    n_symbols += 1

outf.seek(0)

outf.write((n_symbols).to_bytes(8, 'big'))

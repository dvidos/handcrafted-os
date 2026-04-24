
# first stage boot loader

This is 512 bytes stored in the first sector (sector 0) of the disk.

If last two bytes contain the signature 0xAA, 0x55, BIOS will load this 
sector on 0x7C000 and jump to it.

Usually this sector must also share a legacy BIOS partition table,
limiting it even more (four entries, 16 btyes each I think, in offsets 1BE, 1CE, 1DE, 1EE)

UEFI partition tables are located in the second sector.
The first bytes shall contain the signature `EFI PART`.


We plan to have this code to load the second stage loader,
from subsequent sectors from the disk into memory (sectors 2+, onto 0x1000 / 64KB) 
and run it.





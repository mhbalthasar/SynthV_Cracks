/*
 * NOFS Format Specification, 1.0
 *
 * (C) 2023 Xi Jinpwned Software
 *
 * This software may be used by you for non-commercial purposes only.
 */
 
 #include <std/mem.pat>

#pragma MIME application/x-synthv-nofs

#define NOFS_MAGIC 0xF580 /* 0x80, 0xF5, 0x00, 0x00 */

struct NOFSHeader {
    u32 magic [[name("NOFS Magic")]];
    u32 version [[name("Format Version")]];
    u64 fileSize [[name("File Size")]];
};

using Entry;

enum EntryType : u16 {
    Block = 0x0,
    NamedBlock = 0x1,
    MasterTableHeader = 0x1000,
};

struct Block<auto S> {
    u8 data[S];
};

struct NamedBlock<auto S> {
    u16 nameLen [[hidden]];
    char name[nameLen];
    u32 dataLen;
    /* Very slow! */
    //u8 data[dataLen];
};

struct MasterTableEntry {
    u32 id;
    Entry* entry : u64 [[inline]];
};

struct MasterTableHeader<auto S> {
    MasterTableEntry values[while(std::mem::read_unsigned($ + 4, 8) != 0)] [[static, no_unique_address, inline]];
   
    padding[S - 0xc]; 
    u64 trailer;
};

struct Entry {
    u32 size [[name("Entry Size")]];
    EntryType type [[name("Entry Type")]];
    
    u32 dataSize = size - 0x6;
    
    match(type) {
        (EntryType::Block): Block<dataSize> [[inline]];
        (EntryType::NamedBlock): NamedBlock<dataSize> [[inline]];
        (EntryType::MasterTableHeader): MasterTableHeader<dataSize> [[inline]];
    }
};

NOFSHeader header @ 0x0 [[name("NOFS Header")]];
Entry master @ 0x10 [[name("Master Table")]];

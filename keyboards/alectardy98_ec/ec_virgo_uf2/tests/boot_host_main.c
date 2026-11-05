// SPDX-License-Identifier: MIT
static UF2_Block valid(void) {
    UF2_Block b = {.magicStart0=UF2_MAGIC_START0, .magicStart1=UF2_MAGIC_START1,
        .magicEnd=UF2_MAGIC_END, .flags=UF2_FLAG_FAMILYID, .familyID=BOARD_UF2_FAMILY_ID,
        .payloadSize=256, .targetAddr=BOARD_FLASH_APP_START, .blockNo=0, .numBlocks=1};
    memset(b.data,0x5a,256);
    return b;
}
static void rejected(UF2_Block b) {
    WriteState s={0}; unsigned before=writes;
    CHECK(uf2_write_block(0,(uint8_t*)&b,&s)==-1);
    CHECK(writes==before); CHECK(s.numWritten==0);
}
int main(void) {
    void *memory=mmap((void*)0x08000000,256*1024,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    CHECK(memory==(void*)0x08000000);
    uint32_t *start=(void*)BOARD_FLASH_APP_START;
    start[0]=0x20000400; start[1]=0x08011001;
    _board_dfu_dbl_tap[0]=0xf01669ef; host_rcc.CSR=0xa5000000;
    board_init2(); board_dfu_init();
    CHECK(_board_dfu_dbl_tap[0]==0xf01669ef);
    CHECK(host_rcc.CSR==0xa5000000);
    CHECK(dfu_inits==1);
    CHECK(strstr(infoUf2File,"Factory KiB: 256")!=NULL);
    CHECK(strstr(infoUf2File,"Vectors valid: 1")!=NULL);
    CHECK(strstr(infoUf2File,"Request: F01669EF RCC_CSR: A5000000")!=NULL);
    CHECK(strlen(infoUf2File)<sizeof(infoUf2File)-15);
    UF2_Block b=valid(); WriteState s={0};
    CHECK(uf2_write_block(0,(uint8_t*)&b,&s)==512); CHECK(s.numWritten==1);
    CHECK(!memcmp((void*)(uintptr_t)b.targetAddr,b.data,256));
    CHECK(uf2_write_block(0,(uint8_t*)&b,&s)==512); CHECK(s.numWritten==1);
    b.targetAddr=BOARD_FLASH_APP_END-256;
    CHECK(uf2_write_block(0,(uint8_t*)&b,&s)==512);
    uint32_t bad_addr[]={0x08000000,0x08004000,0x08008000,0x0800c000,0x08040000,0xffffffff,0x08010001};
    for(unsigned i=0;i<sizeof(bad_addr)/sizeof(*bad_addr);i++){b=valid();b.targetAddr=bad_addr[i];rejected(b);}
    uint32_t bad_size[]={0,255,257,476,0xffffffff};
    for(unsigned i=0;i<sizeof(bad_size)/sizeof(*bad_size);i++){b=valid();b.payloadSize=bad_size[i];rejected(b);}
    b=valid();b.flags=0;rejected(b);b=valid();b.flags|=1;rejected(b);
    b=valid();b.familyID=0;rejected(b);
    b=valid();b.magicStart0=0;rejected(b);b=valid();b.magicStart1=0;rejected(b);b=valid();b.magicEnd=0;rejected(b);
    unsigned before=unlocks; uint8_t payload[256]={0}; prox_write_failed=false;
    CHECK(!board_flash_write(0x08008000,payload,256)); CHECK(unlocks==before);
    CHECK(!board_app_valid2()); geometry_failed=false;
    density=512;
    CHECK(!board_flash_write(BOARD_FLASH_APP_START,payload,256)); CHECK(unlocks==before);
    CHECK(!board_app_valid()); density=256; geometry_failed=false;
    b=valid();s=(WriteState){0};fail_transfer=true;
    CHECK(uf2_write_block(0,(uint8_t*)&b,&s)==-1);CHECK(s.numWritten==0);CHECK(prox_write_failed);
    board_dfu_complete();CHECK(!resets);fail_transfer=false;prox_write_failed=false;
    memset((void*)BOARD_FLASH_APP_START,0,256);corrupt_transfer=true;
    CHECK(!board_flash_write(BOARD_FLASH_APP_START,b.data,256));CHECK(prox_write_failed);
    corrupt_transfer=false;prox_write_failed=false;board_dfu_complete();CHECK(resets==1);
    uint32_t *vectors=(void*)BOARD_FLASH_APP_START;
    vectors[0]=0x20000400;vectors[1]=0x08011001;CHECK(board_app_valid());
    vectors[1]=0x08011000;CHECK(!board_app_valid());vectors[1]=0x08040001;CHECK(!board_app_valid());
    vectors[1]=0x08000001;CHECK(!board_app_valid());vectors[1]=0x08011001;
    vectors[0]=0x2001fffc;CHECK(!board_app_valid());vectors[0]=0x20020000;CHECK(!board_app_valid());
    vectors[0]=0x20000000;CHECK(!board_app_valid());vectors[0]=0x2001fff8;CHECK(board_app_valid());
    vectors[0]=0x20010000;CHECK(board_app_valid());
    for(unsigned i=0;i<256*1024;i+=256) {
        CHECK(virgo_write_valid(0x08000000+i,256,256)==(i>=0x10000));
    }
    CHECK(!virgo_write_valid(0x08010000,0xffffffff,256));
    // Regression: the old exact-512 density gate rejects the identified CC part.
    CHECK(density==256 && density!=512);
    CHECK(board_app_valid());
    CHECK(virgo_write_valid(BOARD_FLASH_APP_START,256,density));
    CHECK(!virgo_write_valid(0x08040000,256,density));
    printf("PASS: %u assertions; UF2 parser, boot/settings bounds, density, failures, readback, vectors and reset.\n",checks);
    munmap(memory,256*1024);return 0;
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_scsi.h"
#include <string.h>
static uint32_t be32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static uint16_t be16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}
static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
static void fail(my2k_scsi *s, uint8_t key, uint8_t asc) {
    s->sense_key   = key;
    s->sense_asc   = asc;
    s->state       = MY2K_SCSI_FAILED;
    s->data_size   = 0;
    s->blocks_left = 0;
}
static bool capacity(my2k_scsi *s, uint32_t *blocks, uint32_t *gen) {
#if MY2K_DEV7_WRITABLE_MSC
    if(s->ejected)return false;
#endif
    uint16_t size = 0;
    return s->ops.capacity && s->ops.capacity(s->ops.context, blocks, &size, gen) && size == 512 && *blocks;
}
#if MY2K_DEV7_WRITABLE_MSC
static bool writable(my2k_scsi *s,bool arm){
    bool okay=s->ops.writable&&s->ops.submit_write&&s->ops.submit_sync&&s->ops.take&&s->ops.writable(s->ops.context);
    if(okay&&arm)s->writes_armed=true;
    return okay;
}
#endif
void my2k_scsi_init(my2k_scsi *s, const my2k_scsi_ops *ops) {
    if (s) {
        memset(s, 0, sizeof(*s));
        if (ops) s->ops = *ops;
    }
}
static bool shape(my2k_scsi *s, size_t have, size_t wanted, bool in, uint32_t host, uint32_t needed) {
    if (have != wanted || host != needed || (needed && !in)) {
        fail(s, 5, 0x24);
        return false;
    }
    return true;
}
bool my2k_scsi_start(my2k_scsi *s, const uint8_t *c, size_t n, bool in, uint32_t host) {
    if (!s || !c || !n || (s->state != MY2K_SCSI_IDLE && s->state != MY2K_SCSI_PASSED && s->state != MY2K_SCSI_FAILED)) return false;
    s->state       = MY2K_SCSI_PASSED;
    s->data_size   = 0;
    s->blocks_left = 0;
#if MY2K_DEV7_WRITABLE_MSC
    s->write_command=s->sync_command=s->eject_command=false;
#endif
    memset(s->data, 0, sizeof(s->data));
    uint32_t blocks = 0, gen = 0;
    switch (c[0]) {
        case 0x12: /* standard inquiry only, no EVPD */
            if (n != 6) {
                fail(s, 5, 0x24);
                break;
            }
            if (!shape(s, n, 6, in, host, c[4] < 36 ? c[4] : 36)) break;
            if (c[1] || c[2] || c[3] || c[5]) {
                fail(s, 5, 0x24);
                break;
            }
            s->data[1] = 0x80;
            s->data[2] = 4;
            s->data[3] = 2;
            s->data[4] = 31;
            memcpy(s->data + 8, "MY2K EXP", 8);
#if MY2K_DEV7_WRITABLE_MSC
            memcpy(s->data + 16, "Asset bridge    ", 16);
#else
            memcpy(s->data + 16, "Read-only bridge", 16);
#endif
            memcpy(s->data + 32, "0001", 4);
            s->data_size = (uint16_t)host;
            break;
        case 0x03: /* fixed format sense */
            if (n != 6) {
                fail(s, 5, 0x24);
                break;
            }
            if (!shape(s, n, 6, in, host, c[4] < 18 ? c[4] : 18)) break;
            if (c[1] || c[2] || c[3] || c[5]) {
                fail(s, 5, 0x24);
                break;
            }
            s->data[0]   = 0x70;
            s->data[2]   = s->sense_key;
            s->data[7]   = 10;
            s->data[12]  = s->sense_asc;
            s->data_size = (uint16_t)host;
            s->sense_key = 0;
            s->sense_asc = 0;
            break;
        case 0x00:
            if (!shape(s, n, 6, in, host, 0)) break;
            if (c[1] || c[2] || c[3] || c[4] || c[5]) {
                fail(s, 5, 0x24);
                break;
            }
            if (!capacity(s, &blocks, &gen)) fail(s, 2, 0x3a);
            break;
        case 0x1a: /* minimal MODE SENSE(6), write protected, no block descriptors */
            if (n != 6) {
                fail(s, 5, 0x24);
                break;
            }
            if (!shape(s, n, 6, in, host, c[4] < 4 ? c[4] : 4)) break;
            if ((c[1] & ~8u) || (c[2] != 0 && c[2] != 0x3f) || c[3] || c[5]) {
                fail(s, 5, 0x24);
                break;
            }
            s->data[0]   = 3;
#if MY2K_DEV7_WRITABLE_MSC
            s->data[2]   = writable(s,true)?0:0x80;
#else
            s->data[2]   = 0x80;
#endif
            s->data_size = (uint16_t)host;
            break;
        case 0x25:
            if (!shape(s, n, 10, in, host, 8)) break;
            if (c[1] || be32(c + 2) || c[6] || c[7] || c[8] || c[9]) {
                fail(s, 5, 0x24);
                break;
            }
            if (!capacity(s, &blocks, &gen)) {
                fail(s, 2, 0x3a);
                break;
            }
            put32(s->data, blocks - 1);
            put32(s->data + 4, 512);
            s->data_size = 8;
            break;
        case 0x28:
            if (n != 10) {
                fail(s, 5, 0x24);
                break;
            }
            blocks = be16(c + 7);
            if (!shape(s, n, 10, in, host, blocks * 512u)) break;
            if (c[1] || c[6] || c[9]) {
                fail(s, 5, 0x24);
                break;
            }
            s->lba         = be32(c + 2);
            s->blocks_left = blocks;
            if (!capacity(s, &blocks, &s->generation)) {
                fail(s, 2, 0x3a);
                break;
            }
            if (s->lba > blocks || s->blocks_left > blocks - s->lba) {
                fail(s, 5, 0x21);
                break;
            }
            if (s->blocks_left) s->state = MY2K_SCSI_WAIT;
            break;
#if MY2K_DEV7_WRITABLE_MSC
        case 0x2a: { /* WRITE10: one owned512byte block, then physical SYNC. */
            if(n!=10){fail(s,5,0x24);break;}
            uint32_t count=be16(c+7);
            if(host!=count*512u||(host&&in)||(c[1]&~8u)||c[6]||c[9]){fail(s,5,0x24);break;}
            if(!writable(s,true)){fail(s,7,0x27);break;}
            if(!capacity(s,&blocks,&s->generation)){fail(s,2,0x3a);break;}
            s->lba=be32(c+2);s->blocks_left=count;
            if(s->lba>blocks||count>blocks-s->lba){fail(s,5,0x21);break;}
            s->write_command=true;
            if(count)s->state=MY2K_SCSI_OUT;
            break;
        }
        case 0x35: /* SYNCHRONIZE CACHE10: global, non-IMMED barrier only. */
            if(!shape(s,n,10,in,host,0))break;
            if(c[1]||be32(c+2)||c[6]||c[7]||c[8]||c[9]){fail(s,5,0x24);break;}
            if(!capacity(s,&blocks,&s->generation)){fail(s,2,0x3a);break;}
            if(!s->ops.submit_sync||!s->ops.take){fail(s,4,0x44);break;}
            s->sync_command=true;s->state=MY2K_SCSI_WAIT;break;
        case 0x1b: /* START STOP: logical load or stop/eject after actual barrier. */
            if(!shape(s,n,6,in,host,0))break;
            if(c[1]||c[2]||c[3]||(c[4]&~3u)||c[5]){fail(s,5,0x24);break;}
            if(c[4]&1u){
                bool was=s->ejected;s->ejected=false;
                if(!capacity(s,&blocks,&gen)){s->ejected=was;fail(s,2,0x3a);}
                break;
            }
            if(!capacity(s,&blocks,&s->generation)){fail(s,2,0x3a);break;}
            if(!s->ops.submit_sync||!s->ops.take){fail(s,4,0x44);break;}
            s->eject_command=(c[4]&2u)!=0;s->sync_command=true;s->state=MY2K_SCSI_WAIT;break;
#else
        case 0x2a:
#endif
        case 0x15:
        case 0x55:
        case 0x04: /* reject mutations, never dispatch */
            fail(s, 7, 0x27);
            break;
        default:
            fail(s, 5, 0x20);
            break;
    }
    if (s->data_size) s->state = MY2K_SCSI_DATA;
    return true;
}
my2k_scsi_state my2k_scsi_poll(my2k_scsi *s) {
    if (!s) return MY2K_SCSI_FAILED;
    if (s->state == MY2K_SCSI_DRAIN) {
        int32_t result;
        if (s->ops.take && s->ops.take(s->ops.context, s->data, sizeof(s->data), &result)) {
            s->inflight = false;
            s->state    = MY2K_SCSI_IDLE;
        }
    } else if (s->state == MY2K_SCSI_WAIT) {
        uint32_t blocks = 0, gen = 0;
#if MY2K_DEV7_WRITABLE_MSC
        if(s->write_command||s->sync_command){
            if(!s->inflight){
                if(!capacity(s,&blocks,&gen)||gen!=s->generation){fail(s,2,0x3a);return s->state;}
                if(s->write_command){
                    if(!writable(s,false)){fail(s,7,0x27);return s->state;}
                    s->inflight=s->ops.submit_write(s->ops.context,s->lba,s->data);
                }else s->inflight=s->ops.submit_sync&&s->ops.submit_sync(s->ops.context);
            }else{
                int32_t result;
                if(s->ops.take&&s->ops.take(s->ops.context,s->data,sizeof s->data,&result)){
                    s->inflight=false;
                    if(result!=(s->write_command?512:0)){fail(s,3,s->write_command?0x0c:0x44);return s->state;}
                    if(!capacity(s,&blocks,&gen)||gen!=s->generation){fail(s,2,0x3a);return s->state;}
                    if(s->write_command){
                        s->data_size=0;s->lba++;s->blocks_left--;
                        if(s->blocks_left)s->state=MY2K_SCSI_OUT;
                        else{s->write_command=false;s->sync_command=true;}
                    }else{
                        s->sync_command=false;s->state=MY2K_SCSI_PASSED;
                        if(s->eject_command)s->ejected=true;
                    }
                }
            }
            return s->state;
        }
#endif
        if (!s->inflight) {
            if (!capacity(s, &blocks, &gen) || gen != s->generation) {
                fail(s, 2, 0x3a);
                return s->state;
            }
            if (!s->ops.submit || !s->ops.take) {
                fail(s, 4, 0x44);
                return s->state;
            }
#if MY2K_DEV15_PROFILE
            uint16_t limit=s->ops.read_limit?s->ops.read_limit(s->ops.context):512;
            if(!limit||limit>sizeof s->data||(limit%512)){fail(s,4,0x44);return s->state;}
            s->read_bytes=(uint16_t)(s->blocks_left<limit/512u?s->blocks_left*512u:limit);
            s->inflight=s->ops.submit(s->ops.context,s->lba,s->read_bytes);
#else
            s->inflight = s->ops.submit(s->ops.context, s->lba, 512);
#endif
        } else {
            int32_t result;
            if (s->ops.take(s->ops.context, s->data, sizeof(s->data), &result)) {
                s->inflight = false;
                if (result !=
#if MY2K_DEV15_PROFILE
                    s->read_bytes
#else
                    512
#endif
                ) {
                    fail(s, 3, 0x11);
                    return s->state;
                }
                if (!capacity(s, &blocks, &gen) || gen != s->generation) {
                    fail(s, 2, 0x3a);
                    return s->state;
                }
                s->data_size =
#if MY2K_DEV15_PROFILE
                    s->read_bytes;
#else
                    512;
#endif
                s->state     = MY2K_SCSI_DATA;
            }
        }
    }
    return s->state;
}
#if MY2K_DEV15_PROFILE
__attribute__((noipa,externally_visible))
bool my2k_scsi_prefetch(my2k_scsi *s){
    if(!s||s->state!=MY2K_SCSI_DATA||s->inflight||!s->data_size||s->data_size%512u||!s->ops.submit||!s->ops.take)return false;
    uint32_t current=s->data_size/512u,blocks,gen;
    if(s->blocks_left<=current||!capacity(s,&blocks,&gen)||gen!=s->generation)return false;
    uint16_t limit=s->ops.read_limit?s->ops.read_limit(s->ops.context):512;
    if(!limit||limit>sizeof s->data||limit%512u)return false;
    uint32_t left=s->blocks_left-current;
    uint16_t bytes=(uint16_t)(left<limit/512u?left*512u:limit);
    if(!s->ops.submit(s->ops.context,s->lba+current,bytes))return false;
    s->read_bytes=bytes;s->inflight=true;return true;
}
#endif
const uint8_t *my2k_scsi_data(const my2k_scsi *s, uint16_t *size) {
    if (!s || !size || s->state != MY2K_SCSI_DATA) return NULL;
    *size = s->data_size;
    return s->data;
}
void my2k_scsi_consume(my2k_scsi *s) {
    if (!s || s->state != MY2K_SCSI_DATA) return;
    if (s->blocks_left) {
#if MY2K_DEV15_PROFILE
        uint16_t blocks=(uint16_t)(s->data_size/512u);
        s->blocks_left-=blocks;
        s->lba+=blocks;
#else
        s->blocks_left--;
        s->lba++;
#endif
    }
    s->data_size = 0;
    s->state = s->blocks_left ? MY2K_SCSI_WAIT : MY2K_SCSI_PASSED;
}
void my2k_scsi_cancel(my2k_scsi *s) {
    if (!s) return;
    s->data_size   = 0;
    s->blocks_left = 0;
    s->state       = s->inflight ? MY2K_SCSI_DRAIN : MY2K_SCSI_IDLE;
}
#if MY2K_DEV7_WRITABLE_MSC
bool my2k_scsi_out(my2k_scsi *s,const uint8_t *p,size_t n){
    if(!s||!p||!n||n>64||s->state!=MY2K_SCSI_OUT||!s->write_command||n>512u-s->data_size)return false;
    memcpy(s->data+s->data_size,p,n);s->data_size=(uint16_t)(s->data_size+n);
    if(s->data_size==512)s->state=MY2K_SCSI_WAIT;
    return true;
}
bool my2k_scsi_ejected(const my2k_scsi *s){return s&&s->ejected;}
#endif
#if MY2K_DEV7_PROFILE || MY2K_DEV7_WRITABLE_MSC
bool my2k_scsi_export_exit_safe(const my2k_scsi *s){
#if MY2K_DEV7_WRITABLE_MSC
    return s&&(s->ejected||!s->writes_armed);
#else
    return s!=NULL; /* Export cannot accept writes in this compiled profile. */
#endif
}
#endif

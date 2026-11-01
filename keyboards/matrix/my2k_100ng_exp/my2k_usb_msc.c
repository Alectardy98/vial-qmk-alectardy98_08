/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ch.h"
#include "hal.h"
#include "usb_descriptor.h"
#include "my2k_usb_msc.h"
#include "my2k_usb_abort.h"
#include "my2k_usbh_chibios.h"
#include "my2k_bot.h"
#include "my2k_usb_descriptor.h"
#include <string.h>
#if MY2K_DEV16_PROFILE
#include "main_performance_mode.h"
#include "main_usb_performance.h"
#endif
#if MY2K_DEV7_PROFILE
#include "main_storage_mode.h"
static uint8_t asset_device_bytes[18];
static USBDescriptor asset_device_descriptor;
#endif
/* USB hardware is still unverified on Geehy. This middleware chains QMK's
 * original callbacks; all disk requests go through the bounded host worker. */
static const USBConfig  *base_config;
static USBConfig         composite_config;
static USBDescriptor     configuration_descriptor;
static uint8_t           configuration_bytes[512];
static uint8_t           msc_interface, msc_endpoint;
#if MY2K_DEV16_PROFILE
static uint8_t fs_input_bytes[512],hs_input_bytes[512],other_input_bytes[512];
static USBDescriptor fs_input_descriptor,hs_input_descriptor,other_input_descriptor;
static const uint8_t qualifier_bytes[]={10,6,0,2,0,0,0,64,1,0};
static const USBDescriptor qualifier_descriptor={sizeof qualifier_bytes,qualifier_bytes};
#endif
static bool              available, initialized;
static volatile bool     need_reset, reset_accepted;
static volatile bool     aborting;
static my2k_usb_abort    abort_state;
static volatile bool     configured, rx_done, tx_done;
static volatile uint16_t rx_size;
static volatile uint32_t epoch, control_events;
static uint32_t          task_epoch;
static bool              paused;
static uint32_t          pause_started;
/* External MSC and normal animation mode are mutually exclusive. Borrow
 * its existing4KiB DATA buffer for the AFB prefix; no additional RX array. */
static my2k_scsi scsi;
#define afb_prefix_bytes (scsi.data)
static _Alignas(4) uint8_t rx_bytes[64];
static uint16_t afb_header_bytes,afb_rx_remaining;
static volatile bool afb_header_waiting,afb_header_pending;
#if MY2K_DEV15_PROFILE
#include "main_perf.h"
static _Alignas(4) uint8_t tx_bytes[4128];
static uint32_t storage_tx_started;
static bool storage_tx_measured;
#else
static _Alignas(4) uint8_t tx_bytes[64];
#endif
/* AFB1 borrows the existing4096-byte MSC TX buffer only in keyboard mode.
 * IRQ callbacks stage/retire requests; no filesystem or host mutex in IRQ. */
enum {AFB_IDLE,AFB_RX,AFB_QUEUED,AFB_WAIT,AFB_DONE,AFB_RELEASE,AFB_CANCEL,AFB_TX};
static volatile uint8_t afb_stage;
static uint16_t afb_size;
static bool afb_bulk;
static void afb_received(USBDriver *driver);
static void afb_delivered(USBDriver *driver);
static void afb_wake_irq(void){osalSysLockFromISR();my2k_usbh_fast_wakeI();osalSysUnlockFromISR();}
static void afb_received_irq(USBDriver *driver){afb_received(driver);afb_wake_irq();}
bool my2k_usb_animation_bulk_available(void){return !available&&configured&&msc_endpoint;}
static void afb_bulk_rx(USBDriver *driver,usbep_t ep){
 unsigned got=usbGetReceiveTransactionSizeX(driver,ep);
 if(afb_header_waiting){
  afb_header_waiting=false;
  if(got<32||got>sizeof afb_prefix_bytes||afb_prefix_bytes[0]!=253||afb_prefix_bytes[1]!=238||afb_prefix_bytes[3]!=2||!afb_prefix_bytes[4]||(afb_prefix_bytes[2]!=2&&afb_prefix_bytes[2]!=4)){osalSysLockFromISR();usbStallReceiveI(driver,ep);osalSysUnlockFromISR();return;}
  /* EP0 status ACK can precede foreground lease retirement. Reserve only the
   * separate header buffer; never overwrite the previous response or drop the
   * next valid header merely because that response is still retiring. */
  if((afb_prefix_bytes[2]==2&&got!=32)||(afb_prefix_bytes[2]==4&&got>32u+((unsigned)afb_prefix_bytes[30]|(unsigned)afb_prefix_bytes[31]<<8))){osalSysLockFromISR();usbStallReceiveI(driver,ep);osalSysUnlockFromISR();return;}
  afb_header_bytes=(uint16_t)got;afb_header_pending=true;afb_wake_irq();return;
 }
 if(afb_stage!=AFB_RX||!afb_bulk)return;
 if(got!=afb_rx_remaining){osalSysLockFromISR();usbStallReceiveI(driver,ep);osalSysUnlockFromISR();afb_stage=AFB_CANCEL;return;}
 afb_received_irq(driver);
}
static void afb_bulk_tx(USBDriver *driver,usbep_t ep){(void)ep;afb_delivered(driver);}

bool my2k_usb_animation_transfer_busy(void){return afb_stage!=AFB_IDLE||afb_header_pending;}
static uint8_t afb_pending[32];
static uint8_t afb_ready[4]={65,70,66,0};
static void afb_received(USBDriver *driver){
 (void)driver;if(afb_stage!=AFB_RX)return;
 memset(afb_pending,0,32);afb_pending[0]=253;afb_pending[1]=238;afb_pending[2]=170;afb_pending[3]=2;
 afb_pending[4]=tx_bytes[2];afb_pending[5]=tx_bytes[4];afb_pending[6]=1;afb_stage=AFB_QUEUED;
}
static void afb_delivered(USBDriver *driver){(void)driver;if(afb_stage==AFB_DONE||afb_stage==AFB_TX){afb_stage=AFB_RELEASE;afb_wake_irq();}}
static void afb_task(void){
 if(afb_stage==AFB_CANCEL||afb_stage==AFB_RELEASE){if(my2k_usbh_fast_finish(afb_stage==AFB_CANCEL)){afb_stage=AFB_IDLE;afb_bulk=false;}}
 if(afb_stage==AFB_QUEUED){
  if(my2k_usbh_fast_request(tx_bytes,afb_size))afb_stage=AFB_WAIT;
  else{memcpy(tx_bytes,afb_pending,32);tx_bytes[6]=4;afb_size=32;afb_stage=AFB_DONE;}
 }
 if(afb_stage==AFB_WAIT&&my2k_usbh_fast_done(&afb_size))afb_stage=AFB_DONE;
 /* Claim TX while locked, before enabling hardware. Completion can clear the
  * endpoint busy bit immediately; a second service pass must not resend ACK. */
 osalSysLock();
 if(afb_bulk&&afb_stage==AFB_DONE&&configured&&!usbGetTransmitStatusI(&USB_DRIVER,msc_endpoint)){
  afb_stage=AFB_TX;usbStartTransmitI(&USB_DRIVER,msc_endpoint,tx_bytes,afb_size);
 }
 osalSysUnlock();
}
static void afb_listen(void){
 if(!available&&configured&&afb_stage==AFB_IDLE){
  if(afb_header_pending){
   afb_header_pending=false;afb_bulk=true;unsigned prefix=afb_header_bytes-32u;memcpy(tx_bytes,afb_prefix_bytes,afb_header_bytes);afb_size=32;afb_stage=AFB_RX;
   if(tx_bytes[2]==2){afb_received(&USB_DRIVER);return;}
   unsigned n=(unsigned)tx_bytes[30]|(unsigned)tx_bytes[31]<<8;
   if(!n||n>4096||prefix>n){osalSysLock();usbStallReceiveI(&USB_DRIVER,msc_endpoint);osalSysUnlock();afb_stage=AFB_CANCEL;return;}
   afb_size=(uint16_t)(32u+n);afb_rx_remaining=(uint16_t)(n-prefix);
   if(!afb_rx_remaining){afb_received(&USB_DRIVER);return;}
   osalSysLock();usbStartReceiveI(&USB_DRIVER,msc_endpoint,tx_bytes+32+prefix,afb_rx_remaining);osalSysUnlock();return;
  }
  osalSysLock();if(!usbGetReceiveStatusI(&USB_DRIVER,msc_endpoint)){
   afb_header_waiting=true;usbStartReceiveI(&USB_DRIVER,msc_endpoint,afb_prefix_bytes,sizeof afb_prefix_bytes);
  }osalSysUnlock();
 }
}
static USBInEndpointState  in_state;
static USBOutEndpointState out_state;

static my2k_bot            bot;
enum { EVENT_RESET = 1, EVENT_CLEAR_IN = 2, EVENT_CLEAR_OUT = 4 };
#if MY2K_DEV14_PROFILE
/* First16 commands survive BOT recovery; F24 is a new Main boot. No query I/O. */
static volatile uint8_t data0_pending;
static uint8_t trace_count, trace_active=255;
static uint32_t trace_cbws,trace_csws,trace_clear_in,trace_clear_out,trace_resets,trace_max_ms;
static struct { uint8_t opcode,status; uint32_t tag,host,begin,data,queued,done; } trace[16];
static uint32_t trace_word(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void trace_put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void trace_begin(const uint8_t*p,uint16_t size,uint32_t now){
    if(size!=31||trace_word(p)!=0x43425355u)return;
    osalSysLock();trace_cbws++;trace_active=255;
    if(trace_count<16){unsigned i=trace_count++;trace_active=(uint8_t)i;
        memset(&trace[i],0,sizeof trace[i]);trace[i].opcode=p[15];trace[i].status=255;
        trace[i].tag=trace_word(p+4);trace[i].host=trace_word(p+8);trace[i].begin=now;}
    osalSysUnlock();
}
static void trace_tx_I(uint32_t now){
    if(trace_active>=16||trace[trace_active].tag!=bot.tag)return;
    if(bot.pending_status){if(!trace[trace_active].queued)trace[trace_active].queued=now;}
    else if(!trace[trace_active].data)trace[trace_active].data=now;
}
static void trace_complete(uint32_t now){
    if(!bot.pending_status)return;
    osalSysLock();trace_csws++;
    uint32_t elapsed=now-bot.started_ms;if(elapsed>trace_max_ms)trace_max_ms=elapsed;
    if(trace_active<16&&trace[trace_active].tag==bot.tag){trace[trace_active].done=now;trace[trace_active].status=bot.status;}
    trace_active=255;osalSysUnlock();
}
bool my2k_usb_msc_diag(uint8_t page,uint8_t index,uint8_t report[32]){
    osalSysLock();
    if((page==25&&index)||(page==26&&index>=trace_count)){osalSysUnlock();return false;}
    memset(report+4,0,28);report[4]=1;
    if(page==25){report[5]=trace_count;report[6]=16;report[7]=(available?1:0)|(configured?2:0)|(aborting?4:0);
        trace_put(report+8,trace_cbws);trace_put(report+12,trace_csws);trace_put(report+16,trace_clear_in);
        trace_put(report+20,trace_clear_out);trace_put(report+24,trace_resets);trace_put(report+28,trace_max_ms);
    }else if(page==26){report[5]=index;report[6]=trace[index].opcode;report[7]=trace[index].status;
        const uint32_t values[]={trace[index].tag,trace[index].host,trace[index].begin,trace[index].data,trace[index].queued,trace[index].done};
        for(unsigned i=0;i<6;i++)trace_put(report+8+4*i,values[i]);
    }else{osalSysUnlock();return false;}
    osalSysUnlock();return true;
}
#endif
/* Main MSC endpoints only. A BOT class reset alone must preserve toggles.
 * Reset DATA0 only to honor an accepted host CLEAR_FEATURE(HALT), including
 * one deferred until endpoint abort/drain completes. Never reset HID toggles. */
static void clear_msc_halt(USBDriver *driver,bool in,bool data0){
    if(in){usb_lld_clear_in(driver,msc_endpoint);
#if MY2K_DEV14_PROFILE
        if(data0)driver->otg->ie[msc_endpoint].DIEPCTL|=DIEPCTL_SD0PID;
#endif
    }else{usb_lld_clear_out(driver,msc_endpoint);
#if MY2K_DEV14_PROFILE
        if(data0)driver->otg->oe[msc_endpoint].DOEPCTL|=DOEPCTL_SD0PID;
#endif
    }
    (void)data0;
}

static void tx_callback(USBDriver *driver, usbep_t endpoint) {
    (void)driver;
    (void)endpoint;
    tx_done = true;
}
static void rx_callback(USBDriver *driver, usbep_t endpoint) {
    rx_size = (uint16_t)usbGetReceiveTransactionSizeX(driver, endpoint);
    rx_done = true;
}
static const USBEndpointConfig endpoint_configuration = {.ep_mode = USB_EP_MODE_TYPE_BULK, .setup_cb = NULL, .in_cb = tx_callback, .out_cb = rx_callback, .in_maxsize = 64, .out_maxsize = 64, .in_state = &in_state, .out_state = &out_state, .in_multiplier = 2, .setup_buf = NULL};
static const USBEndpointConfig afb_fs_endpoint={.ep_mode=USB_EP_MODE_TYPE_BULK,.in_cb=afb_bulk_tx,.out_cb=afb_bulk_rx,.in_maxsize=64,.out_maxsize=64,.in_state=&in_state,.out_state=&out_state,.in_multiplier=2};
static const USBEndpointConfig afb_hs_endpoint={.ep_mode=USB_EP_MODE_TYPE_BULK,.in_cb=afb_bulk_tx,.out_cb=afb_bulk_rx,.in_maxsize=512,.out_maxsize=512,.in_state=&in_state,.out_state=&out_state,.in_multiplier=2};

static const USBDescriptor    *descriptor_callback(USBDriver *driver, uint8_t dtype, uint8_t index, uint16_t language) {
#if MY2K_DEV16_PROFILE
    if(index==0){
        bool high=my2k_performance_usb_speed()==2;
        if(dtype==2&&!available)return high?&hs_input_descriptor:&fs_input_descriptor;
        if(dtype==6&&my2k_performance_hs_requested())return &qualifier_descriptor;
        if(dtype==7&&my2k_performance_hs_requested()){
            const USBDescriptor *opposite=high?&fs_input_descriptor:&hs_input_descriptor;
            memcpy(other_input_bytes,opposite->ud_string,opposite->ud_size);other_input_bytes[1]=7;
            other_input_descriptor=(USBDescriptor){opposite->ud_size,other_input_bytes};return &other_input_descriptor;
        }
    }
#endif
    if (available && dtype == 2 && index == 0) return &configuration_descriptor;
#if MY2K_DEV7_PROFILE
    if(available && dtype==1 && index==0){
        const USBDescriptor *base=base_config->get_descriptor_cb(driver,dtype,index,language);
        if(base && base->ud_size==18 && base->ud_string[0]==18 && base->ud_string[1]==1){
            memcpy(asset_device_bytes,base->ud_string,18);
            asset_device_bytes[10]=0xfc;asset_device_bytes[11]=0xf1;
            asset_device_descriptor=(USBDescriptor){18,asset_device_bytes};return &asset_device_descriptor;
        }
        return base;
    }
#endif
    return base_config->get_descriptor_cb(driver, dtype, index, language);
}
static void event_callback(USBDriver *driver, usbevent_t event) {
#if MY2K_DEV16_PROFILE
    if(event==USB_EVENT_RESET)my2k_performance_usb_reset();
    else if(event==USB_EVENT_UNCONFIGURED||event==USB_EVENT_SUSPEND)my2k_performance_usb_configured(false);
    else if(event==USB_EVENT_CONFIGURED)my2k_performance_usb_configured(true);
    else if(event==USB_EVENT_WAKEUP)my2k_performance_usb_configured(driver->configuration!=0);
#endif
    if(event==USB_EVENT_RESET||event==USB_EVENT_UNCONFIGURED){afb_header_waiting=afb_header_pending=false;if(afb_stage!=AFB_IDLE)afb_stage=AFB_CANCEL;afb_wake_irq();}
    base_config->event_cb(driver, event);
    if (event == USB_EVENT_CONFIGURED) {
        osalSysLockFromISR();
        usbInitEndpointI(driver,msc_endpoint,available?&endpoint_configuration:(my2k_performance_usb_speed()==2?&afb_hs_endpoint:&afb_fs_endpoint));
        rx_done = tx_done = false;
        control_events    = 0;
#if MY2K_DEV14_PROFILE
        data0_pending=0;
#endif
        need_reset = reset_accepted = false;
        epoch++;
        configured = true;
        osalSysUnlockFromISR();
    } else if (event == USB_EVENT_RESET || event == USB_EVENT_UNCONFIGURED) {
        configured = false;
#if MY2K_DEV14_PROFILE
        data0_pending=0;
#endif
        epoch++;
    } else if (event == USB_EVENT_SUSPEND) {
        configured = false;
    } else if (event == USB_EVENT_WAKEUP && driver->configuration) {
        configured = true;
    }
}
static bool request_callback(USBDriver *driver) {
    const uint8_t *s     = driver->setup;
    uint16_t       value = (uint16_t)(s[2] | (uint16_t)s[3] << 8), index = (uint16_t)(s[4] | (uint16_t)s[5] << 8), length = (uint16_t)(s[6] | (uint16_t)s[7] << 8);
    static uint8_t lun;
    if(!available&&!my2k_asset_export_active()&&!value&&!index){
        if(s[0]==0xc0&&s[1]==0x62&&length==4){
            afb_ready[3]=afb_header_pending?AFB_QUEUED:afb_stage;usbSetupTransfer(driver,afb_ready,4,NULL);return true;
        }
        if(s[0]==0x40&&s[1]==0x60&&length>=32&&length<=sizeof tx_bytes){
            if(afb_stage!=AFB_IDLE)return false;
            afb_bulk=false;afb_size=length;afb_stage=AFB_RX;
            usbSetupTransfer(driver,tx_bytes,length,afb_received_irq);return true;
        }
        if(s[0]==0xc0&&s[1]==0x61&&(length==4096||length==sizeof tx_bytes)){
            if(afb_stage==AFB_DONE){usbSetupTransfer(driver,tx_bytes,afb_size,afb_delivered);return true;}
            if(afb_stage==AFB_WAIT||afb_stage==AFB_QUEUED){usbSetupTransfer(driver,afb_pending,32,NULL);return true;}
            return false;
        }
    }
    if (available && index == msc_interface && (s[0] == 0xa1 || s[0] == 0x21)) {
        if (s[0] == 0xa1 && s[1] == 0xfe && value == 0 && length == 1) {
            usbSetupTransfer(driver, &lun, 1, NULL);
            return true;
        }
        if (s[0] == 0x21 && s[1] == 0xff && value == 0 && length == 0) {
#if MY2K_DEV14_PROFILE
            trace_resets++;
#endif
            control_events = EVENT_RESET;
            reset_accepted = true;
            usbSetupTransfer(driver, NULL, 0, NULL);
            return true;
        }
        return false;
    }
    if (available && s[0] == 0x02 && s[1] == 1 && value == 0 && length == 0 && ((index == msc_endpoint) || (index == (uint16_t)(0x80 | msc_endpoint)))) {
        bool in = (index & 0x80) != 0;
        /* Invalid-CBW halt needs BOT reset before ClearFeature may release it. */
#if MY2K_DEV14_PROFILE
        if(!need_reset||reset_accepted){
            data0_pending|=in?1u:2u;
            if(in)trace_clear_in++;else trace_clear_out++;
        }
#endif
        if ((!need_reset || reset_accepted) && !aborting && !(control_events & EVENT_RESET)) {
            clear_msc_halt(driver,in,true);
#if MY2K_DEV14_PROFILE
            data0_pending&=~(in?1u:2u);
#endif
        }
        control_events |= in ? EVENT_CLEAR_IN : EVENT_CLEAR_OUT;
        usbSetupTransfer(driver, NULL, 0, NULL);
        return true;
    }
    return base_config->requests_hook_cb ? base_config->requests_hook_cb(driver) : false;
}
msg_t my2k_usb_start(USBDriver *driver, const USBConfig *configuration) {
#if MY2K_DEV7_PROFILE
    my2k_asset_mode_init();
#endif
#if MY2K_DEV16_PROFILE
    my2k_performance_init();
#endif
    base_config      = configuration;
    composite_config = *configuration;
    if (!initialized) {
        my2k_usbh_scsi_init(&scsi);
        my2k_bot_init(&bot, &scsi);
        initialized = true;
    } else
        my2k_bot_disconnect(&bot);
    const void *base                   = NULL;
    uint16_t    size                   = get_usb_descriptor(0x0200, 0, 0xffff, &base);
    size_t      length                 = 0;
    available                          =
#if MY2K_DEV7_PROFILE
        my2k_asset_export_active() &&
#endif
        my2k_usb_append_msc(base, size, USB_MAX_ENDPOINTS, configuration_bytes, sizeof configuration_bytes, &length, &msc_interface, &msc_endpoint);
#if MY2K_DEV16_PROFILE
    size_t fs_size=0,hs_size=0;
    if(!my2k_input_usb_configuration(base,size,false,false,KEYBOARD_IN_EPNUM,SHARED_IN_EPNUM,fs_input_bytes,sizeof fs_input_bytes,&fs_size)||
       !my2k_input_usb_configuration(base,size,true,false,KEYBOARD_IN_EPNUM,SHARED_IN_EPNUM,hs_input_bytes,sizeof hs_input_bytes,&hs_size)){
        my2k_performance_usb_fault(8u);return (msg_t)-1;
    }
    if(!available){
      uint8_t iface,endpoint;size_t count;
      if(!my2k_usb_append_msc(fs_input_bytes,fs_size,USB_MAX_ENDPOINTS,configuration_bytes,sizeof configuration_bytes,&count,&iface,&endpoint))return (msg_t)-1;
      configuration_bytes[fs_size+5]=255;configuration_bytes[fs_size+6]=0xaf;configuration_bytes[fs_size+7]=3;
      memcpy(fs_input_bytes,configuration_bytes,count);fs_size=count;msc_interface=iface;msc_endpoint=endpoint;
      if(!my2k_usb_append_msc(hs_input_bytes,hs_size,USB_MAX_ENDPOINTS,configuration_bytes,sizeof configuration_bytes,&count,&iface,&endpoint)||iface!=msc_interface||endpoint!=msc_endpoint)return (msg_t)-1;
      configuration_bytes[hs_size+5]=255;configuration_bytes[hs_size+6]=0xaf;configuration_bytes[hs_size+7]=3;
      configuration_bytes[hs_size+13]=0;configuration_bytes[hs_size+14]=2;configuration_bytes[hs_size+20]=0;configuration_bytes[hs_size+21]=2;
      memcpy(hs_input_bytes,configuration_bytes,count);hs_size=count;
    }
    fs_input_descriptor=(USBDescriptor){fs_size,fs_input_bytes};hs_input_descriptor=(USBDescriptor){hs_size,hs_input_bytes};
#endif
    configuration_descriptor.ud_string = configuration_bytes;
    configuration_descriptor.ud_size   = length;
#if MY2K_DEV14_PROFILE
    data0_pending=0;
#endif
    configured                         = false;
    aborting                           = false;
    need_reset = reset_accepted = false;
    epoch++;
    rx_done = tx_done = false;
    control_events    = 0;
    if (available
#if MY2K_DEV16_PROFILE
        || true
#endif
    ) {
        composite_config.event_cb          = event_callback;
        composite_config.get_descriptor_cb = descriptor_callback;
        composite_config.requests_hook_cb  = request_callback;
    }
    return usbStart(driver, &composite_config);
}
/* The existing media worker is the unique thread owner of the borrowed buffer.
 * USB IRQs only publish bounded state and signal its existing semaphore. This
 * removes QMK/RGB housekeeping latency without another thread or large buffer. */
__attribute__((noinline,noclone,externally_visible)) void my2k_usb_animation_service(void){
 if(initialized){afb_task();afb_listen();afb_task();}
}
void my2k_usb_msc_task(uint32_t now) {
    if(!initialized)return;

    if(!available)return;
    uint8_t  input[64];
    uint16_t received;
    bool     input_done, output_done, active;
    osalSysLock();
    uint32_t current_epoch = epoch, events = control_events;
    control_events = 0;
    active         = configured;
    input_done     = rx_done;
    output_done    = tx_done;
    received       = rx_size;
    if (input_done && received <= sizeof input) memcpy(input, rx_bytes, received);
    rx_done = tx_done = false;
    osalSysUnlock();
    /* No host-worker mutex or protocol callback runs under the USB systemlock. */
    if (task_epoch != current_epoch) {
        task_epoch = current_epoch;
        my2k_bot_disconnect(&bot);
        aborting   = false;
        paused     = false;
#if MY2K_DEV15_PROFILE
        storage_tx_measured=false;
#endif
        input_done = output_done = false;
    }
    if (events & EVENT_RESET) {
        my2k_bot_class_reset(&bot);
        my2k_usb_abort_begin(&abort_state, now);
        aborting   = true;
        input_done = output_done = false;
    }
    if (events & EVENT_CLEAR_IN) my2k_bot_clear_halt(&bot, true);
    if (events & EVENT_CLEAR_OUT) my2k_bot_clear_halt(&bot, false);
    if (active && !aborting && !(control_events & EVENT_RESET)) {
        if (output_done) {
#if MY2K_DEV15_PROFILE
            if(storage_tx_measured){my2k_perf_record(MY2K_PERF_STORAGE_TX,storage_tx_started);storage_tx_measured=false;}
#endif
#if MY2K_DEV14_PROFILE
            trace_complete(now);
#endif
            my2k_bot_in_complete(&bot);
        }
        if (input_done) {
#if MY2K_DEV14_PROFILE
            trace_begin(input,received,now);
#endif
            my2k_bot_out(&bot, input, received, now);
        }
    }
    if (!active && !paused) {
        paused        = true;
        pause_started = now;
    }
    if (active && paused) {
        bot.started_ms += (uint32_t)(now - pause_started);
        paused = false;
    }
    if (active) my2k_bot_poll(&bot, now);
    osalSysLock();
    need_reset = bot.stage == MY2K_BOT_RECOVERY;
    if (!need_reset) reset_accepted = false;
    if (current_epoch != epoch || !configured || (control_events & EVENT_RESET) || usbGetDriverStateI(&USB_DRIVER) != USB_ACTIVE) {
        osalSysUnlock();
        return;
    }
    if (aborting) {
        if (my2k_usb_abort_pollI(&abort_state, &USB_DRIVER, msc_endpoint, now)) {
            aborting = false;
            rx_done = tx_done = false;
#if MY2K_DEV14_PROFILE
            if (!bot.stall_in){clear_msc_halt(&USB_DRIVER,true,(data0_pending&1)!=0);data0_pending&=~1u;}
            if (!bot.stall_out){clear_msc_halt(&USB_DRIVER,false,(data0_pending&2)!=0);data0_pending&=~2u;}
#else
            if (!bot.stall_in) usb_lld_clear_in(&USB_DRIVER, msc_endpoint);
            if (!bot.stall_out) usb_lld_clear_out(&USB_DRIVER, msc_endpoint);
#endif
        }
        osalSysUnlock();
        return;
    }
    if (bot.stall_in) usbStallTransmitI(&USB_DRIVER, msc_endpoint);
    if (bot.stall_out) usbStallReceiveI(&USB_DRIVER, msc_endpoint);
    if (!usbGetTransmitStatusI(&USB_DRIVER, msc_endpoint)) {
        uint16_t       bytes;
        const uint8_t *data = my2k_bot_peek(&bot, &bytes);
        if (data) {
#if MY2K_DEV14_PROFILE
            trace_tx_I(now);
#endif
#if MY2K_DEV15_PROFILE
            storage_tx_measured=!bot.pending_status&&scsi.blocks_left;
            if(storage_tx_measured)storage_tx_started=my2k_perf_clock();
#endif
            memcpy(tx_bytes, data, bytes);
            usbStartTransmitI(&USB_DRIVER, msc_endpoint, tx_bytes, bytes);
        }
    }
    if (my2k_bot_wants_out(&bot) && !rx_done && !usbGetReceiveStatusI(&USB_DRIVER, msc_endpoint)) usbStartReceiveI(&USB_DRIVER, msc_endpoint, rx_bytes, 64);
    osalSysUnlock();
}

#if MY2K_RELEASE1_PROFILE
bool my2k_usb_msc_exit_safe(void){return my2k_usbh_write_profile_ready()?(my2k_scsi_ejected(&scsi)&&bot.stage==MY2K_BOT_CBW&&!bot.pending&&!rx_done&&!tx_done):my2k_scsi_export_exit_safe(&scsi);}
#endif

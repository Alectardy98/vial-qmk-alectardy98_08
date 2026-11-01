#!/usr/bin/env python3
"""Pinned localTinyUSB adaptation: asynchronous negotiatedC3 reads only."""
from pathlib import Path
import hashlib,json,sys
source,out=map(Path,sys.argv[1:]);raw=source.read_bytes();assert hashlib.sha256(raw).hexdigest()=='d99c13717255880b336e5223c8cfa98581a0fa449cf6372983dfc808e13be4da'
text=raw.decode();changes=[]
def replace(old,new):
 global text
 assert text.count(old)==1,(old,text.count(old));text=text.replace(old,new);changes.append(dict(before=old,after=new))
replace("    case SCSI_CMD_WRITE_10:\n      proc_write_io_data(p_msc, (uint32_t) nbytes, nbytes);\n      break;\n\n    default: break; // nothing to do", """    case SCSI_CMD_WRITE_10:
      proc_write_io_data(p_msc, (uint32_t) nbytes, nbytes);
      break;

    case 0xc3:
      if (p_msc->stage != MSC_STAGE_DATA || nbytes <= 0 || (uint32_t)nbytes != p_msc->total_len || (uint32_t)nbytes > CFG_TUD_MSC_EP_BUFSIZE) {
        fail_scsi_op(p_msc, MSC_CSW_STATUS_FAILED);
      } else if (!usbd_edpt_xfer(p_msc->rhport, p_msc->ep_in, _mscd_epbuf.buf, (uint16_t)nbytes, false)) {
        fail_scsi_op(p_msc, MSC_CSW_STATUS_FAILED);
      }
      break;
    default: break; // nothing to do""")
replace("      if ((SCSI_CMD_READ_10 == p_cbw->command[0]) || (SCSI_CMD_WRITE_10 == p_cbw->command[0])) {", """      if (p_cbw->command[0] == 0xc3) {
        /* Only this negotiated vendor read may defer a custom IN command.
         * Reject wrong direction/oversize BEFORE invoking the application. */
        if (!is_data_in(p_cbw->dir) || !p_cbw->total_bytes || p_cbw->total_bytes > CFG_TUD_MSC_EP_BUFSIZE) {
          fail_scsi_op(p_msc, MSC_CSW_STATUS_FAILED);
        } else {
          int32_t result = tud_msc_scsi_cb(p_cbw->lun, p_cbw->command, _mscd_epbuf.buf, (uint16_t)p_cbw->total_bytes);
          if (result == TUD_MSC_RET_ASYNC) p_msc->pending_io = true;
          else fail_scsi_op(p_msc, MSC_CSW_STATUS_FAILED);
        }
      } else if ((SCSI_CMD_READ_10 == p_cbw->command[0]) || (SCSI_CMD_WRITE_10 == p_cbw->command[0])) {""")
out.write_text(text);out.with_suffix('.json').write_text(json.dumps(dict(upstream=str(source),upstream_sha256='d99c13717255880b336e5223c8cfa98581a0fa449cf6372983dfc808e13be4da',generated_sha256=hashlib.sha256(text.encode()).hexdigest(),changes=changes),indent=2)+'\n')

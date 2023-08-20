#include <assert.h>
#include <errno.h>
#include <scsi/sg_lib.h>
#include <scsi/sg_pt.h>
#include <string.h>

#include "sd_usb.h"

#define SCSI_PASS_THROUGH_RETRY
#define SWAP_ENDIAN(x)                                                         \
  (((x & 0xFF000000) >> 24) | ((x & 0x00FF0000) >> 8) |                        \
   ((x & 0x0000FF00) << 8) | ((x & 0x000000FF) << 24))

int verbose = 1; /* debug */
int debug = 1;

int sd_get_cid(int hFile, int something);

static int SCSIPassThroughDirect(int hFile, void *pCDB, uint8_t nCDBLength,
                                 __attribute__((unused))
                                 SCSI_PASS_THROUGH_STATUS *sptsStatus,
                                 void *pBuffer, uint32_t dwBufferLength,
                                 int bWrite);

static int SCSIPassThroughDirect(int hFile, void *pCDB, uint8_t nCDBLength,
                                 __attribute__((unused))
                                 SCSI_PASS_THROUGH_STATUS *sptsStatus,
                                 void *pBuffer, uint32_t dwBufferLength,
                                 int bWrite) {
  uint8_t sense_buffer[32];
  struct sg_pt_base *ptvp;
  int result, resid, category, sense_len;
  char buffer[512];

  assert(dwBufferLength != 1);

  memset(sense_buffer, 0, sizeof(sense_buffer));
  memset(buffer, 0, sizeof(buffer));

  /* construct a SCSI pass through object */
  ptvp = construct_scsi_pt_obj();
  /* set our desired CDB */
  set_scsi_pt_cdb(ptvp, pCDB, nCDBLength);
  /* set the sense return buffer */
  set_scsi_pt_sense(ptvp, sense_buffer, sizeof(sense_buffer));

  /* set the data in/out/no-data for this CDB */
  if (dwBufferLength == 0) {
    /* default is no data */
    ;
  } else if (bWrite == TRUE) {
    set_scsi_pt_data_out(ptvp, pBuffer, dwBufferLength);
  } else {
    set_scsi_pt_data_in(ptvp, pBuffer, dwBufferLength);
  }

  /* send the pass through command */
  result = do_scsi_pt(ptvp, hFile, SCSI_PASS_THROUGH_TIMEOUT, verbose);
  if (result < 0) {
    printf("do_scsi_pt result %d\n", result);
  } else if (result == SCSI_PT_DO_BAD_PARAMS) {
    printf("SCSI_PT_DO_BAD_PARAMS\n");
  } else if (result == SCSI_PT_DO_TIMEOUT) {
    printf("SCSI_PT_DO_TIMEOUT\n");
  }

  resid = get_scsi_pt_resid(ptvp);
  category = get_scsi_pt_result_category(ptvp);
  switch (category) {
  case SCSI_PT_RESULT_GOOD:
    /* bytes returned is dwBufferLength - resid */
    if (debug) {
      printf("DEBUG: buffer size %d, got %d\n", dwBufferLength,
             dwBufferLength - resid);
    }
    break;
  case SCSI_PT_RESULT_STATUS:
    /* get result string */
    sg_get_scsi_status_str(get_scsi_pt_status_response(ptvp), sizeof(buffer),
                           buffer);
    printf("SCSI_PT_RESULT_STATUS: scsi status: %s\n", buffer);
    destruct_scsi_pt_obj(ptvp);
    return FALSE;
    break;
  case SCSI_PT_RESULT_SENSE:
    /* handle sense code */
    sense_len = get_scsi_pt_sense_len(ptvp);
    sg_get_sense_str("", sense_buffer, sense_len, (verbose > 1), sizeof(buffer),
                     buffer);
    printf("SCSI_PT_RESULT_SENSE: sense %s", buffer);
    destruct_scsi_pt_obj(ptvp);
    return FALSE;
    break;
  case SCSI_PT_RESULT_TRANSPORT_ERR:
    get_scsi_pt_transport_err_str(ptvp, sizeof(buffer), buffer);
    printf("SCSI_PT_RESULT_TRANSPORT_ERR: %s\n", buffer);
    destruct_scsi_pt_obj(ptvp);
    return FALSE;
    break;
  case SCSI_PT_RESULT_OS_ERR:
    get_scsi_pt_os_err_str(ptvp, sizeof(buffer), buffer);
    printf("SCSI_PT_RESULT_OS_ERR: %s\n", buffer);
    destruct_scsi_pt_obj(ptvp);
    return FALSE;
    break;
  default:
    printf("unknown result: category %d\n", category);
    destruct_scsi_pt_obj(ptvp);
    return FALSE;
  }

  destruct_scsi_pt_obj(ptvp);

  return TRUE;
}

int sd_get_cid(int hFile, int something) {
  int status = 0;
  SD_OVER_USB_CDB soucCDB;
  uint8_t value[16];
  int i = 0;

  bzero(&soucCDB, sizeof(soucCDB));
  memset(value, 0xff, 16);

  soucCDB.SD_VENDOR.OperationCode = 0xCF;
  soucCDB.SD_VENDOR.vendorCdb.mVend_Func_Code = 0x18;
  soucCDB.SD_VENDOR.vendorCdb.mReserved3 = 16;
  soucCDB.SD_VENDOR.vendorCdb.mReserved1 = 0;
  soucCDB.SD_VENDOR.vendorCdb.mReserved2 = 0;
  soucCDB.SD_VENDOR.vendorCdb.mReserved4 = 0;

  status = SCSIPassThroughDirect(hFile, &soucCDB, 16, NULL, value, 16, FALSE);
  printf("%s::%d:status is %d\n", __FUNCTION__, __LINE__, status);

  for (i = 0; i < 16; i++) {
    printf("CID[%d] is %02x\n", i, value[i]);
  }
  return status;
}

int main(int argc, char *argv[]) {

  int hFile = -1;
  int bRet = FALSE;
  CDB cdbCDB;
  SD_INQUIRY_DATA souidData;
  INQUIRYDATA iInquiryData;
  char *pfix_dev_name;

  /* pfix-add Get the device name from the command line */
  if (argc == 1) {
    printf("Usage: sd_usb /dev/your-dev-name\n");
    return -1;
  }
  pfix_dev_name = argv[1];

  ZeroMemory(&cdbCDB, sizeof(CDB));
  ZeroMemory(&souidData, sizeof(souidData));
  ZeroMemory(&iInquiryData, sizeof(INQUIRYDATA));

  /* read only, verbose */
  hFile = scsi_pt_open_device(pfix_dev_name, 1, 1);
  if (hFile < 0) {
    printf("open device %s failed %d\n", pfix_dev_name, errno);
    return -1;
  }

  /* Standard SCSI Inquiry */
  printf("\n*** Doing standard SCSI Inquiry ***\n");

  cdbCDB.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
  cdbCDB.CDB6INQUIRY3.AllocationLength = INQUIRYDATABUFFERSIZE;

  bRet =
      SCSIPassThroughDirect(hFile, &cdbCDB, sizeof(cdbCDB.CDB6INQUIRY3), NULL,
                            &iInquiryData, INQUIRYDATABUFFERSIZE, FALSE);
  if (bRet != TRUE) {
    printf("SCI Inquiry failed\n");
    CloseHandle(hFile);
    return -1;
  }
  /* dump out the data */
  printf("SCSI Inquiry:\n");
  printf("  Vendor Id %s\n", iInquiryData.VendorId);
  printf("  Product Id %s\n", iInquiryData.ProductId);
  printf("  Revision %s\n", iInquiryData.ProductRevisionLevel);
  sd_get_cid(hFile, 0);
  printf("%s::%d: _____________\n", __FUNCTION__, __LINE__);

  return 0;
}

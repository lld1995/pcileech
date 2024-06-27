#include "pcileechlib.h"
#include "pcileech.h"
#include "device.h"
#include "executor.h"
#include "extra.h"
#include "help.h"
#include "memdump.h"
#include "mempatch.h"
#include "util.h"
#include "kmd.h"
#include "umd.h"
#include "vfs.h"
#include "vmmx.h"
extern PPCILEECH_CONTEXT ctxMain;

void StopDump() {
    StopMemoryDump();
}

int StartDump(OnProgressNotify opn,const char* outPath) {
    ctxMain = LocalAlloc(LMEM_ZEROINIT, sizeof(PCILEECH_CONTEXT));
    if (!ctxMain) {
        return 1;
    }
    ctxMain->magic = PCILEECH_CONTEXT_MAGIC;
    ctxMain->version = PCILEECH_CONTEXT_VERSION;
    ctxMain->cfg.tpAction = DUMP;
    ctxMain->cfg.paAddrMax = 0;
    ctxMain->cfg.fOutFile = TRUE;
    ctxMain->cfg.fUserInteract = TRUE;

#if _DEBUG
    ctxMain->cfg.fVerbose = TRUE;
    ctxMain->cfg.fVerboseExtra = TRUE;
    ctxMain->cfg.fVerboseExtraTlp = TRUE;
#endif

    strcpy_s(ctxMain->cfg.szDevice, MAX_PATH, "FPGA");
    if (outPath == NULL) {

    }
    else {
        strcpy_s(ctxMain->cfg.szFileOut, MAX_PATH, outPath);
    }

    if (!ctxMain->cfg.pbIn) {
        ctxMain->cfg.pbIn = LocalAlloc(LMEM_ZEROINIT, 0x40000);
    }
    // set dummy qwAddrMax value (if possible) to disable auto-detect in LeechCore.
    if ((ctxMain->cfg.tpAction == TLP) || (ctxMain->cfg.tpAction == DISPLAY) || (ctxMain->cfg.tpAction == PAGEDISPLAY)) {
        ctxMain->cfg.paAddrMax = -1;
    }
    // disable memory auto-detect when memmap is specified
    if (!ctxMain->cfg.paAddrMax && (ctxMain->cfg.szMemMap[0] || ctxMain->cfg.szMemMapStr[0])) {
        ctxMain->cfg.paAddrMax = -1;
    }
    // try correct erroneous options, if needed
    /*if (ctxMain->cfg.tpAction == NA) {
        return FALSE;
    }*/
    // set vamax
    if (!ctxMain->cfg.vaAddrMax) {
        ctxMain->cfg.vaAddrMax = (QWORD)-1;
    }

    PKMDEXEC pKmdExec = NULL;
    BOOL result;
    if (ctxMain->cfg.tpAction == EXEC_KMD) {
        result = Util_LoadKmdExecShellcode(ctxMain->cfg.szShellcodeName, &pKmdExec);
        LocalFree(pKmdExec);
        if (!result) {
            Help_ShowGeneral();
            PCILeechFreeContext();
            return 1;
        }
    }
    // actions that do not require a working initialized connection to a pcileech
    // device to start executing the command are found below:
    if (ctxMain->cfg.tpAction == INFO || ctxMain->cfg.tpAction == MAC_FVRECOVER2 || ctxMain->cfg.tpAction == MAC_DISABLE_VTD || ctxMain->cfg.fShowHelp) {
        if (ctxMain->cfg.tpAction == INFO) {
            Help_ShowInfo();
        }
        else if (ctxMain->cfg.tpAction == MAC_FVRECOVER2) {
            Action_MacFilevaultRecover(FALSE);
        }
        else if (ctxMain->cfg.tpAction == MAC_DISABLE_VTD) {
            Action_MacDisableVtd();
        }
        else if (ctxMain->cfg.fShowHelp) {
            Help_ShowDetailed();
        }
        PCILeechFreeContext();
        return 0;
    }
    result = DeviceOpen();
    if (!result) {
        printf("PCILEECH: Failed to connect to the device.\n");
        PCILeechFreeContext();
        return 1;
    }
    if (ctxMain->cfg.fBarZeroReadWrite) {
        // pcileech implementation of a zero read/write PCIe BAR.
        Extra_BarReadWriteInitialize();
    }
    else if (ctxMain->cfg.fBarZeroReadOnly) {
        // use leechcore implementation of a zero read-only PCIe BAR.
        if (!LcCommand(ctxMain->hLC, LC_CMD_FPGA_BAR_FUNCTION_CALLBACK, 0, (PBYTE)LC_BAR_FUNCTION_CALLBACK_ZEROBAR, NULL, NULL)) {
            printf("BAR: Error registering callback function and enabling BAR TLP processing.\n");
        }
    }
    PCILeechConfigFixup(); // post device config adjustments
    if (ctxMain->cfg.szKMDName[0] || ctxMain->cfg.paKMD) {
        result = KMDOpen();
        if (!result) {
            printf("PCILEECH: Failed to load kernel module.\n");
            PCILeechFreeContext();
            return 1;
        }
    }
    if (ctxMain->cfg.paAddrMax == 0) {
        LcGetOption(ctxMain->hLC, LC_OPT_CORE_ADDR_MAX, &ctxMain->cfg.paAddrMax);
    }
    // enable ctrl+c event handler if remote (to circumvent blocking thread)
    PCILeechCtrlHandlerInitialize();

    ActionMemoryDump(opn);

    if (ctxMain && ctxMain->phKMD && (ctxMain->cfg.tpAction != KMDLOAD) && !ctxMain->cfg.fAddrKMDSetByArgument) {
        KMDUnload();
        printf("KMD: Hopefully unloaded.\n");
    }
    if (ctxMain && ctxMain->cfg.dwListenTlpTimeMs) {
        Sleep(50);
        LcCommand(ctxMain->hLC, LC_CMD_FPGA_TLP_FUNCTION_CALLBACK, 0, (PBYTE)LC_TLP_FUNCTION_CALLBACK_DUMMY, NULL, NULL);
        Sleep(ctxMain->cfg.dwListenTlpTimeMs);
        LcCommand(ctxMain->hLC, LC_CMD_FPGA_TLP_FUNCTION_CALLBACK, 0, (PBYTE)LC_TLP_FUNCTION_CALLBACK_DISABLE, NULL, NULL);
    }
    PCILeechFreeContext();
    return 0;
//#ifdef LINUX
//    ExitProcess(0);
//#else /* LINUX */
//    __try {
//        ExitProcess(0);
//    }
//    __except (EXCEPTION_EXECUTE_HANDLER) { ; }
//#endif /* LINUX */
}
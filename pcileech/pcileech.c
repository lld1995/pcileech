// pcileech.c : implementation of core pcileech functionality.
//
// (c) Ulf Frisk, 2016-2022
// Author: Ulf Frisk, pcileech@frizk.net
//
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
PPCILEECH_CONTEXT ctxMain = NULL;

BOOL PCILeechConfigIntialize(_In_ DWORD argc, _In_ char* argv[])
{
    struct ACTION {
        ACTION_TYPE tp;
        LPSTR sz;
    } ACTION;
    const struct ACTION ACTIONS[] = {
        {.tp = NONE,.sz = "none"},
        {.tp = BENCHMARK,.sz = "benchmark"},
        {.tp = INFO,.sz = "info"},
        {.tp = DUMP,.sz = "dump" },
        {.tp = WRITE,.sz = "write" },
        {.tp = PATCH,.sz = "patch" },
        {.tp = SEARCH,.sz = "search" },
        {.tp = KMDLOAD,.sz = "kmdload" },
        {.tp = KMDEXIT,.sz = "kmdexit" },
        {.tp = MOUNT,.sz = "mount" },
        {.tp = DISPLAY,.sz = "display" },
        {.tp = PAGEDISPLAY,.sz = "pagedisplay" },
        {.tp = TESTMEMREAD,.sz = "testmemread" },
        {.tp = TESTMEMREADWRITE,.sz = "testmemreadwrite" },
        {.tp = MAC_FVRECOVER,.sz = "mac_fvrecover" },
        {.tp = MAC_FVRECOVER2,.sz = "mac_fvrecover2" },
        {.tp = MAC_DISABLE_VTD,.sz = "mac_disablevtd" },
        {.tp = PT_PHYS2VIRT,.sz = "pt_phys2virt" },
        {.tp = PT_VIRT2PHYS,.sz = "pt_virt2phys" },
        {.tp = TLP,.sz = "tlp" },
        {.tp = TLPLOOP,.sz = "tlploop" },
        {.tp = PROBE,.sz = "probe" },
        {.tp = REGCFG,.sz = "regcfg" },
        {.tp = PSLIST,.sz = "pslist" },
        {.tp = PSVIRT2PHYS,.sz = "psvirt2phys" },
        {.tp = AGENT_EXEC_PY,.sz = "agent-execpy" },
        {.tp = AGENT_FORENSIC,.sz = "agent-forensic"},
    };
    DWORD j, i = 1;
    FILE *hFile;
    CHAR szCommandModule[MAX_PATH];
    ctxMain = LocalAlloc(LMEM_ZEROINIT, sizeof(PCILEECH_CONTEXT));
    if(!ctxMain) {
        return 1;
    }
    ctxMain->magic = PCILEECH_CONTEXT_MAGIC;
    ctxMain->version = PCILEECH_CONTEXT_VERSION;
    if(argc < 2) { return FALSE; }
    // set defaults
    ctxMain->argc = argc;
    ctxMain->argv = argv;
    ctxMain->cfg.tpAction = NA;
    ctxMain->cfg.paAddrMax = 0;
    ctxMain->cfg.fOutFile = TRUE;
    ctxMain->cfg.fUserInteract = TRUE;
    // fetch command line actions/options
    loop:
    while(i < argc) {
        // try parse action command
        for(j = 0; j < sizeof(ACTIONS) / sizeof(ACTION); j++) {
            if(0 == _stricmp(argv[i], ACTIONS[j].sz)) {
                ctxMain->cfg.tpAction = ACTIONS[j].tp;
                i++;
                goto loop;
            }
        }
        // try parse external command module name
        if((ctxMain->cfg.tpAction == NA) && (0 != memcmp(argv[i], "-", 1))) {
            Util_GetPathExe(szCommandModule);
            if(strlen(szCommandModule) + strlen(argv[i]) < MAX_PATH - 16) {
                strcat_s(szCommandModule, sizeof(szCommandModule), "leechp_");
                strcat_s(szCommandModule, sizeof(szCommandModule), argv[i]);
                strcat_s(szCommandModule, sizeof(szCommandModule), PCILEECH_LIBRARY_FILETYPE);
                if(0 == fopen_s(&hFile, szCommandModule, "rb")) {
                    memcpy(ctxMain->cfg.szExternalCommandModule, szCommandModule, MAX_PATH);
                    ctxMain->cfg.tpAction = EXTERNAL_COMMAND_MODULE;
                    fclose(hFile);
                    i++;
                    continue;
                }
            }
        }
        // try parse 
        if((ctxMain->cfg.tpAction == NA) && (0 != memcmp(argv[i], "-", 1))) {
            ctxMain->cfg.tpAction = ((strlen(argv[i]) > 3) && !_strnicmp("umd", argv[i], 3)) ? EXEC_UMD : EXEC_KMD;
            strcpy_s(ctxMain->cfg.szShellcodeName, MAX_PATH, argv[i]);
            i++;
            continue;
        }
        // parse options (command not found)
        if(0 == strcmp(argv[i], "-pt")) {
            ctxMain->cfg.fPageTableScan = TRUE;
            i++;
            continue;
        } else if(0 == strcmp(argv[i], "-all")) {
            ctxMain->cfg.fPatchAll = TRUE;
            i++;
            continue;
        } else if(0 == _stricmp(argv[i], "-bar-ro")) {
            ctxMain->cfg.fBarZeroReadOnly = TRUE;
            i++;
            continue;
        } else if(0 == _stricmp(argv[i], "-bar-rw")) {
            ctxMain->cfg.fBarZeroReadWrite = TRUE;
            i++;
            continue;
        } else if(0 == strcmp(argv[i], "-force")) {
            ctxMain->cfg.fForceRW = TRUE;
            i++;
            continue;
        } else if(0 == strcmp(argv[i], "-help")) {
            ctxMain->cfg.fShowHelp = TRUE;
            i++;
            continue;
        } else if(0 == _stricmp(argv[i], "-v")) {
            ctxMain->cfg.fVerbose = TRUE;
            i++;
            continue;
        } else if(0 == _stricmp(argv[i], "-vv")) {
            ctxMain->cfg.fVerboseExtra = TRUE;
            i++;
            continue;
        } else if(0 == _stricmp(argv[i], "-vvv")) {
            ctxMain->cfg.fVerboseExtraTlp = TRUE;
            i++;
            continue;
        } else if(0 == _stricmp(argv[i], "-loop")) {
            ctxMain->cfg.fLoop = TRUE;
            i++;
            continue;
        } else if(0 == strcmp(argv[i], "-nouserinteract")) {
            ctxMain->cfg.fUserInteract = FALSE;
            i++;
            continue;
        } else if(i + 1 >= argc) {
            return FALSE;
        } else if(0 == strcmp(argv[i], "-min")) {
            ctxMain->cfg.paAddrMin = Util_GetNumeric(argv[i + 1]);
        } else if(0 == strcmp(argv[i], "-max")) {
            ctxMain->cfg.paAddrMax = Util_GetNumeric(argv[i + 1]);
        } else if(0 == strcmp(argv[i], "-pid")) {
            ctxMain->cfg.dwPID = (DWORD)Util_GetNumeric(argv[i + 1]);
            ctxMain->cfg.fModeVirtual = ctxMain->cfg.dwPID ? TRUE : FALSE;
        } else if(0 == strcmp(argv[i], "-vamin")) {
            ctxMain->cfg.vaAddrMin = Util_GetNumeric(argv[i + 1]);
        } else if(0 == strcmp(argv[i], "-vamax")) {
            ctxMain->cfg.vaAddrMax = Util_GetNumeric(argv[i + 1]);
        } else if(0 == strcmp(argv[i], "-psname")) {
            strcpy_s(ctxMain->cfg.szProcessName, MAX_PATH, argv[i + 1]);
            ctxMain->cfg.fModeVirtual = ctxMain->cfg.szProcessName[0] ? TRUE : FALSE;
        } else if(0 == strcmp(argv[i], "-cr3")) {
            ctxMain->cfg.paCR3 = Util_GetNumeric(argv[i + 1]);
        } else if(0 == strcmp(argv[i], "-efibase")) {
            ctxMain->cfg.paEFI_IBI_SYST = Util_GetNumeric(argv[i + 1]);
        } else if(0 == strcmp(argv[i], "-tlpwait")) {
            ctxMain->cfg.dwListenTlpTimeMs = (DWORD)(1000 * Util_GetNumeric(argv[i + 1]));
        } else if((0 == strcmp(argv[i], "-device")) || (0 == strcmp(argv[i], "-z"))) {
            strcpy_s(ctxMain->cfg.szDevice, MAX_PATH, argv[i + 1]);
        } else if(0 == strcmp(argv[i], "-remote")) {
            strcpy_s(ctxMain->cfg.szRemote, MAX_PATH, argv[i + 1]);
        } else if(0 == strcmp(argv[i], "-memmap")) {
            strcpy_s(ctxMain->cfg.szMemMap, MAX_PATH, argv[i + 1]);
        } else if(0 == _stricmp(argv[i], "-memmap-str")) {
            strcpy_s(ctxMain->cfg.szMemMapStr, _countof(ctxMain->cfg.szMemMapStr), argv[i + 1]);
            i += 2;
            continue;
        } else if(0 == strcmp(argv[i], "-out")) {
            if((0 == _stricmp(argv[i + 1], "none")) || (0 == _stricmp(argv[i + 1], "null"))) {
                ctxMain->cfg.fOutFile = FALSE;
            } else {
                strcpy_s(ctxMain->cfg.szFileOut, MAX_PATH, argv[i + 1]);
            }
        } else if(0 == strcmp(argv[i], "-in")) {
            ctxMain->cfg.cbIn = max(0x40000, 0x1000 + Util_GetFileSize(argv[i + 1]));
            ctxMain->cfg.pbIn = LocalAlloc(LMEM_ZEROINIT, (SIZE_T)ctxMain->cfg.cbIn);
            if(!ctxMain->cfg.pbIn) { return FALSE; }
            if(!Util_ParseHexFileBuiltin(argv[i + 1], ctxMain->cfg.pbIn, (DWORD)ctxMain->cfg.cbIn, (PDWORD)&ctxMain->cfg.cbIn)) { return FALSE; }
        } else if(0 == strcmp(argv[i], "-s")) {
            strcpy_s(ctxMain->cfg.szInS, MAX_PATH, argv[i + 1]);
        } else if(0 == strcmp(argv[i], "-mount")) {
            strcpy_s(ctxMain->cfg.szMount, MAX_PATH, argv[i + 1]);
        } else if(0 == strcmp(argv[i], "-sig")) {
            strcpy_s(ctxMain->cfg.szSignatureName, MAX_PATH, argv[i + 1]);
        } else if(0 == strcmp(argv[i], "-hook")) {
            strcpy_s(ctxMain->cfg.szHook, MAX_PATH, argv[i + 1]);
        } else if(0 == strcmp(argv[i], "-kmd")) {
            ctxMain->cfg.paKMD = strtoull(argv[i + 1], NULL, 16);
            if(ctxMain->cfg.paKMD < 0x1000) {
                strcpy_s(ctxMain->cfg.szKMDName, MAX_PATH, argv[i + 1]);
            } else {
                ctxMain->cfg.fAddrKMDSetByArgument = TRUE;
            }
        } else if(2 == strlen(argv[i]) && '0' <= argv[i][1] && '9' >= argv[i][1]) { // -0..9 param
            ctxMain->cfg.qwDataIn[argv[i][1] - '0'] = Util_GetNumeric(argv[i + 1]);
        }
        i += 2;
    }
    if(!ctxMain->cfg.pbIn) {
        ctxMain->cfg.pbIn = LocalAlloc(LMEM_ZEROINIT, 0x40000);
    }
    // set dummy qwAddrMax value (if possible) to disable auto-detect in LeechCore.
    if((ctxMain->cfg.tpAction == TLP) || (ctxMain->cfg.tpAction == DISPLAY) || (ctxMain->cfg.tpAction == PAGEDISPLAY)) {
        ctxMain->cfg.paAddrMax = -1;
    }
    // disable memory auto-detect when memmap is specified
    if(!ctxMain->cfg.paAddrMax && (ctxMain->cfg.szMemMap[0] || ctxMain->cfg.szMemMapStr[0])) {
        ctxMain->cfg.paAddrMax = -1;
    }
    // try correct erroneous options, if needed
    if(ctxMain->cfg.tpAction == NA) {
        return FALSE;
    }
    // set vamax
    if(!ctxMain->cfg.vaAddrMax) {
        ctxMain->cfg.vaAddrMax = (QWORD)-1;
    }
    return TRUE;
}

VOID PCILeechConfigFixup()
{
    QWORD qw;
    // no kmd -> max address == max address that device support
    if(!ctxMain->cfg.szKMDName[0] && !ctxMain->cfg.paKMD) {
        if(ctxMain->cfg.paAddrMax == 0 || ctxMain->cfg.paAddrMax > ctxMain->dev.paMax) {
            ctxMain->cfg.paAddrMax = ctxMain->dev.paMax;
        }
    }
    // fixup addresses
    if(ctxMain->cfg.paAddrMin > ctxMain->cfg.paAddrMax) {
        qw = ctxMain->cfg.paAddrMin;
        ctxMain->cfg.paAddrMin = ctxMain->cfg.paAddrMax;
        ctxMain->cfg.paAddrMax = qw;
    }
    ctxMain->cfg.paCR3 &= ~0xfff;
    ctxMain->cfg.paKMD &= ~0xfff;
}

VOID PCILeechFreeContext()
{
    if(!ctxMain) { return; }
    ActionUnMount();
    KMDClose();
    Vmmx_Close();
    LcClose(ctxMain->hLC);
    LocalFree(ctxMain->cfg.pbIn);
    LocalFree(ctxMain);
    ctxMain = NULL;
}

#ifdef _WIN32
/*
* Call the free context functionality in a separate thread (in case it gets stuck).
* -- pv
*/
VOID WINAPI PCILeechCtrlHandler_TryShutdownThread(PVOID pv)
{
	__try {
		PCILeechFreeContext();
	} __except(EXCEPTION_EXECUTE_HANDLER) { ; }
}

/*
* SetConsoleCtrlHandler for PCILeech - clean up whenever CTRL+C is pressed.
* -- fdwCtrlType
* -- return
*/
BOOL WINAPI PCILeechCtrlHandler(DWORD fdwCtrlType)
{
    if(fdwCtrlType == CTRL_C_EVENT) {
        printf("CTRL+C detected - shutting down ...\n");
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PCILeechCtrlHandler_TryShutdownThread, NULL, 0, NULL);
		Sleep(500);
		TerminateProcess(GetCurrentProcess(), 1);
		Sleep(1000);
		ExitProcess(1);
        return TRUE;
    }
    return FALSE;
}

VOID PCILeechCtrlHandlerInitialize()
{
	SetConsoleCtrlHandler(PCILeechCtrlHandler, TRUE);
}
#endif /* _WIN32 */

#ifdef LINUX
VOID PCILeechCtrlHandlerInitialize()
{
	return;
}
#endif /* LINUX */

int main(_In_ int argc, _In_ char* argv[])
{
    BOOL result;
    HMODULE hExternalCommandModule;
    VOID(*pfnExternalCommandModuleDoAction)(_Inout_ PPCILEECH_CONTEXT pLeechContext);
    PKMDEXEC pKmdExec = NULL;
    result = PCILeechConfigIntialize((DWORD)argc, argv);
    printf("\n");
    if(!result) {
        Help_ShowGeneral();
        PCILeechFreeContext();
        return 1;
    }
    if(ctxMain->cfg.tpAction == EXEC_KMD) {
        result = Util_LoadKmdExecShellcode(ctxMain->cfg.szShellcodeName, &pKmdExec);
        LocalFree(pKmdExec);
        if(!result) {
            Help_ShowGeneral();
            PCILeechFreeContext();
            return 1;
        }
    }
    // actions that do not require a working initialized connection to a pcileech
    // device to start executing the command are found below:
    if(ctxMain->cfg.tpAction == INFO || ctxMain->cfg.tpAction == MAC_FVRECOVER2 || ctxMain->cfg.tpAction == MAC_DISABLE_VTD || ctxMain->cfg.fShowHelp) {
        if(ctxMain->cfg.tpAction == INFO) {
            Help_ShowInfo();
        } else if(ctxMain->cfg.tpAction == MAC_FVRECOVER2) {
            Action_MacFilevaultRecover(FALSE);
        } else if(ctxMain->cfg.tpAction == MAC_DISABLE_VTD) {
            Action_MacDisableVtd();
        } else if(ctxMain->cfg.fShowHelp) {
            Help_ShowDetailed();
        }
        PCILeechFreeContext();
        return 0;
    }
    result = DeviceOpen();
    if(!result) {
        printf("PCILEECH: Failed to connect to the device.\n");
        PCILeechFreeContext();
        return 1;
    }
    if(ctxMain->cfg.fBarZeroReadWrite) {
        // pcileech implementation of a zero read/write PCIe BAR.
        Extra_BarReadWriteInitialize();
    } else if(ctxMain->cfg.fBarZeroReadOnly) {
        // use leechcore implementation of a zero read-only PCIe BAR.
        if(!LcCommand(ctxMain->hLC, LC_CMD_FPGA_BAR_FUNCTION_CALLBACK, 0, (PBYTE)LC_BAR_FUNCTION_CALLBACK_ZEROBAR, NULL, NULL)) {
            printf("BAR: Error registering callback function and enabling BAR TLP processing.\n");
        }
    }
    PCILeechConfigFixup(); // post device config adjustments
    if(ctxMain->cfg.szKMDName[0] || ctxMain->cfg.paKMD) {
        result = KMDOpen();
        if(!result) {
            printf("PCILEECH: Failed to load kernel module.\n");
            PCILeechFreeContext();
            return 1;
        }
    }
    if(ctxMain->cfg.paAddrMax == 0) {
        LcGetOption(ctxMain->hLC, LC_OPT_CORE_ADDR_MAX, &ctxMain->cfg.paAddrMax);
    }
    // enable ctrl+c event handler if remote (to circumvent blocking thread)
	PCILeechCtrlHandlerInitialize();
    // main dispatcher
    switch(ctxMain->cfg.tpAction) {
        case NONE:
            break;
        case BENCHMARK:
            Action_Benchmark();
            break;
        case DUMP:
            ActionMemoryDump(NULL,FALSE);
            break;
        case WRITE:
            ActionMemoryWrite();
            break;
        case DISPLAY:
            if(ctxMain->cfg.fModeVirtual) {
                ActionMemoryDisplayVirtual();
            } else {
                ActionMemoryDisplayPhysical();
            }
            break;
        case PAGEDISPLAY:
            ActionMemoryPageDisplay();
            break;
        case PATCH:
        case SEARCH:
            if(ctxMain->cfg.fModeVirtual) {
                ActionPatchAndSearchVirtual();
            } else {
                ActionPatchAndSearchPhysical();
            }
            break;
        case EXEC_KMD:
            ActionExecShellcode();
            break;
        case EXEC_UMD:
            ActionExecUserMode();
            break;
        case TESTMEMREAD:
        case TESTMEMREADWRITE:
            ActionMemoryTestReadWrite();
            break;
        case MAC_FVRECOVER:
            Action_MacFilevaultRecover(TRUE);
            break;
        case PT_PHYS2VIRT:
            Action_PT_Phys2Virt();
            break;
        case PT_VIRT2PHYS:
            Action_PT_Virt2Phys();
            break;
        case TLP:
            Action_TlpTx();
            break;
        case TLPLOOP:
            Action_TlpTxLoop();
            break;
        case PROBE:
            ActionMemoryProbe();
            break;
        case REGCFG:
            Action_RegCfgReadWrite();
            break;
        case MOUNT:
            ActionMount();
            break;
        case PSLIST:
            Action_UmdPsList();
            break;
        case PSVIRT2PHYS:
            Action_UmdPsVirt2Phys();
            break;
        case AGENT_EXEC_PY:
            ActionAgentExecPy();
            break;
        case AGENT_FORENSIC:
            ActionAgentForensic();
            break;
        case KMDLOAD:
            if(ctxMain->cfg.paKMD) {
                printf("KMD: Successfully loaded at address: 0x%08x\n", (DWORD)ctxMain->cfg.paKMD);
            } else {
                printf("KMD: Failed. Please supply valid -kmd and optionally -cr3 parameters.\n");
            }
            break;
        case KMDEXIT:
            KMDUnload();
            printf("KMD: Hopefully unloaded.\n");
            break;
        case EXTERNAL_COMMAND_MODULE:
            if((hExternalCommandModule = LoadLibraryA(ctxMain->cfg.szExternalCommandModule))) {
                if((pfnExternalCommandModuleDoAction = (VOID(*)(PPCILEECH_CONTEXT))GetProcAddress(hExternalCommandModule, "DoAction"))) {
                    pfnExternalCommandModuleDoAction(ctxMain);
                } else {
                    printf("Failed. External command module '%s' could not locate required DoAction function.\n", ctxMain->cfg.szExternalCommandModule);
                }
                FreeLibrary(hExternalCommandModule);
            } else {
                printf("Failed. External command module '%s' could not be loaded.\n", ctxMain->cfg.szExternalCommandModule);
            }
            break;
        default:
            printf("Failed. Not yet implemented.\n");
            break;
    }
    if(ctxMain && ctxMain->phKMD && (ctxMain->cfg.tpAction != KMDLOAD) && !ctxMain->cfg.fAddrKMDSetByArgument) {
        KMDUnload();
        printf("KMD: Hopefully unloaded.\n");
    }
    if(ctxMain && ctxMain->cfg.dwListenTlpTimeMs) {
        Sleep(50);
        LcCommand(ctxMain->hLC, LC_CMD_FPGA_TLP_FUNCTION_CALLBACK, 0, (PBYTE)LC_TLP_FUNCTION_CALLBACK_DUMMY, NULL, NULL);
        Sleep(ctxMain->cfg.dwListenTlpTimeMs);
        LcCommand(ctxMain->hLC, LC_CMD_FPGA_TLP_FUNCTION_CALLBACK, 0, (PBYTE)LC_TLP_FUNCTION_CALLBACK_DISABLE, NULL, NULL);
    }
    PCILeechFreeContext();
#ifdef LINUX
    ExitProcess(0);
#else /* LINUX */
    __try {
        ExitProcess(0);
    } __except(EXCEPTION_EXECUTE_HANDLER) { ; }
#endif /* LINUX */
    return 0;
}

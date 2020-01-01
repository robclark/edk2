/** @file

  Copyright (c) 2019, Linaro. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <libfdt.h>
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "Common.h"
#include "Qcom.h"

/*
 * We (at least currently) don't care about most of the fields, just
 * panel_id:
 */
typedef struct {
  UINT32 VersionInfo;
  UINT32 pad0[9];
  UINT32 PanelId;
  UINT32 pad1[17];
} MdpDispInfo;

#define MDP_DISP_INFO_VERSION_MAGIC 0xaa

/**
  Try to load and apply panel-overlay dtb
 */
STATIC
EFI_STATUS
QcomLoadPanelOverlay(
  IN EFI_FILE_PROTOCOL *Root,
  IN VOID              *Blob,
  IN CHID              CurrentCHID,
  IN UINT32            PanelId
  )
{
  EFI_STATUS Status;
  EFI_GUID   CHID;
  VOID       *OverlayBlob;
  INT32      Err;

  Status = GetComputerHardwareId(&CHID, CurrentCHID);
  ASSERT (!EFI_ERROR (Status));

  /* First try \dtb\%g-panel-%x.dtb, using the CHID that was used to find
   * the in-used fdt, ie. if main .dtb is:
   *
   *    \dtb\30b031c0-9de7-5d31-a61c-dee772871b7d.dtb
   *
   * Then the first path we try is:
   *
   *    \dtb\30b031c0-9de7-5d31-a61c-dee772871b7d-panel-$PanelId.dtb
   *
   * This should probably never be required, but in case a panel-id does
   * get re-cycled between different products, this lets us first interpret
   * the panel specific to the device, but considering the global namespace:
   */
  Status = ReadFdt (&OverlayBlob, Root, L"\\dtb\\%g-panel-%x.dtb", &CHID, PanelId);
  if (EFI_ERROR (Status)) {
    /* Then try \dtb\qcom-panels\panel-$PanelId.dtb ... This is where we expect
     * to normally find the panel, since the panel-id's seem to be a flat/global
     * namespace.
     */
    Status = ReadFdt (
        &OverlayBlob,
        Root,
        L"\\dtb\\qcom-panels\\panel-%x.dtb",
        PanelId
        );
  }

  if (!EFI_ERROR (Status)) {
    Dbg (L"Found panel overlay!\n");

    Err = fdt_overlay_apply (Blob, OverlayBlob);
    if (Err) {
      Print (L"Could not apply overlay: %a\n", fdt_strerror (Err));
      Status = EFI_OUT_OF_RESOURCES;
    } else {
      Dbg (L"Panel overlay applied successfully!\n");
    }
  } else {
    Dbg (L"Could not find panel overlay! (%x)\n", Status);
  }

  return Status;
}

/**
  Detect (if present) the qcom specific UEFIDisplayInfo variable, adjust dtb
  accordingly

  @param[in] Root         The root of filesys that Blob was loaded from (ie.
                          where to look for overlays)
  @param[in] Blob         The fdt blob to patch if panel id is detected
  @param[in] CurrentCHID  The CHID used to construct path that blob was loaded
                          from

  @retval EFI_SUCCESS          The panel was detected and dt patched successfully.
  @retval other                Error.
**/
EFI_STATUS
EFIAPI
QcomDetectPanel (
  IN EFI_FILE_PROTOCOL *Root,
  IN VOID              *Blob,
  IN CHID              CurrentCHID
  )
{
  EFI_STATUS    Status;
  MdpDispInfo   *DispInfo;

  Status = GetVariable3 (
      L"UEFIDisplayInfo",
      &gEfiGraphicsOutputProtocolGuid,
      (VOID **) &DispInfo,
      NULL,
      NULL
      );
  if (EFI_ERROR (Status)) {
    Print (L"%a:%d: Status = %x\n", __func__, __LINE__, Status);
    return Status;
  }

  Dbg (L"Got VersionInfo: 0x%08x\n", DispInfo->VersionInfo);

  if ((DispInfo->VersionInfo >> 16) != MDP_DISP_INFO_VERSION_MAGIC) {
    Print (L"Bad VersionInfo magic: 0x%08x\n", DispInfo->VersionInfo);
    goto Cleanup;
  }

  Dbg (L"Got PanelId: 0x%x\n", DispInfo->PanelId);

  Status = QcomLoadPanelOverlay (Root, Blob, CurrentCHID, DispInfo->PanelId);

 Cleanup:
  FreePool (DispInfo);
  return Status;
}

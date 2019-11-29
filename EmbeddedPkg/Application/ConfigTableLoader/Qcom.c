/** @file

  Copyright (c) 2019, Linaro. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <libfdt.h>
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "Common.h"

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
  IN UINT32            PanelId
  )
{
  EFI_STATUS Status;
  VOID       *OverlayBlob;
  CHAR16     PanelIdStr[8 + 1];
  INT32      Err;

  UnicodeSPrint (PanelIdStr, sizeof (PanelIdStr), L"%x", PanelId);

  Dbg (L"PanelIdStr=%s\n", PanelIdStr);

  /* First try \dtb\$SysVendor\$ProductName-$BoardName-panel-$PanelId.dtb */
  Status = TryLoadBlob (
      Root,
      (CHAR16 *[]){ L"\\dtb", mSmbiosInfo.SysVendor }, 2,
      (CHAR16 *[]){ mSmbiosInfo.ProductName, mSmbiosInfo.BoardName, L"panel", PanelIdStr }, 4,
      &OverlayBlob
      );
  if (EFI_ERROR (Status)) {
    /* Then try \dtb\$SysVendor\$ProductName-panel-$PanelId.dtb */
    Status = TryLoadBlob (
        Root,
        (CHAR16 *[]){ L"\\dtb", mSmbiosInfo.SysVendor }, 2,
        (CHAR16 *[]){ mSmbiosInfo.ProductName, L"panel", PanelIdStr }, 3,
        &OverlayBlob
        );
  }

  if (EFI_ERROR (Status)) {
    /* Finally try \dtb\panels\panel-$PanelId.dtb ... This is where we expect
     * to normally find the panel, since the panel-id's seem to be a flat/global
     * namespace.  The earlier attempts are just to provide an override mechanism
     * in case we later discover that a panel-id has been reused
     */
    Status = TryLoadBlob (
        Root,
        (CHAR16 *[]){ L"\\dtb\\panels" }, 1,
        (CHAR16 *[]){ L"panel", PanelIdStr }, 2,
        &OverlayBlob
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
  }

  return Status;
}

/**
  Detect (if present) the qcom specific UEFIDisplayInfo variable, adjust dtb
  accordingly

  @param[in] Blob  The fdt blob to patch if panel id is detected

  @retval EFI_SUCCESS          The panel was detected and dt patched successfully.
  @retval other                Error.

**/
EFI_STATUS
EFIAPI
QcomDetectPanel (
  IN EFI_FILE_PROTOCOL *Root,
  IN VOID              *Blob
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

  Status = QcomLoadPanelOverlay (Root, Blob, DispInfo->PanelId);

 Cleanup:
  FreePool (DispInfo);
  return Status;
}

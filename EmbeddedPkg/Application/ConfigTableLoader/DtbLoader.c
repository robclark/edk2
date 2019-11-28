/** @file
  Application to load and register a .dtb file.

  Replaces any existing registration.

  Copyright (c) 2019, Linaro. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <libfdt.h>
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Guid/Fdt.h>

#include "Common.h"

STATIC struct {
  UINT32 Crc32;
  UINT32 TotalSize;
  VOID   *Data;
} mBlobInfo;


STATIC
EFI_STATUS
RegisterDtBlob (
  IN VOID *Blob
  )
{
  EFI_STATUS Status;

  /* Calculate CRC to detect changes.  The linux kernel's efi libstub
   * will insert the kernel commandline into the chosen node before
   * calling ExitBootServices, and we can use this to differentiate
   * between ACPI boot (ie. windows) and DT boot.
   */
  mBlobInfo.TotalSize = fdt_totalsize (mBlobInfo.Data);
  gBS->CalculateCrc32 (mBlobInfo.Data, mBlobInfo.TotalSize, &mBlobInfo.Crc32);

  Print (L"DT CRC32: %08x\n", mBlobInfo.Crc32);
  Print (L"DT TotalSize: %d bytes\n", mBlobInfo.TotalSize);

  Status = gBS->InstallConfigurationTable (&gFdtTableGuid, Blob);
  if (!EFI_ERROR (Status)) {
    Print (L"DTB installed successfully!\n");
  }

  return Status;
}


STATIC
VOID
EFIAPI
ExitBootServicesHook (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  VOID *Data;
  UINT32 Crc32;

#if !defined(MDEPKG_NDEBUG)
  gST->ConOut->OutputString (gST->ConOut,
		 L"Checking DT CRC...\r\n"
		 );
#endif

  // If the table we registered isn't there, abort.
  if (EFI_ERROR (EfiGetSystemConfigurationTable (&gFdtTableGuid, &Data))) {
    return;
  }

  gBS->CalculateCrc32 (Data, fdt_totalsize (Data), &Crc32);
  if (Crc32 == mBlobInfo.Crc32) {
    // If Crc32 unchanged, ACPI is in use, so don't delete it.
    return;
  }

#if !defined(MDEPKG_NDEBUG)
  gST->ConOut->OutputString (gST->ConOut,
		 L"DT in use - unregistering ACPI tables\r\n"
		 );
#endif

  // DT appears to be used, so deregister ACPI tables
  gBS->InstallConfigurationTable (&gEfiAcpiTableGuid, NULL);
  gBS->InstallConfigurationTable (&gEfiAcpi20TableGuid, NULL);
}

/**
  The user Entry Point for Application. The user code starts with this function
  as the real entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_LOADED_IMAGE_PROTOCOL       *LoadedImage;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
  EFI_FILE_PROTOCOL               *Root;
  EFI_STATUS                      Status;
  VOID                            *Blob;

  Status = GetLoadedImageProtocol (&LoadedImage);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = GetLoadedImageFileSystem (LoadedImage, &FileSystem);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = FileSystem->OpenVolume (FileSystem, &Root);
  if (EFI_ERROR (Status)) {
    Print (L"OpenVolume call failed!\n");
    goto Cleanup;
  }

  /* fallback to trying \MY.DTB: */
  Status = ReadFdt (&Blob, Root, L"\\MY.dtb");
  if (!EFI_ERROR (Status)) {
    EFI_EVENT ExitBootServicesEvent;

    RegisterDtBlob (Blob);

    Status = gBS->CreateEvent (
		    EVT_SIGNAL_EXIT_BOOT_SERVICES,
		    TPL_CALLBACK,
		    ExitBootServicesHook,
		    NULL,
		    &ExitBootServicesEvent
		    );
    if (EFI_ERROR (Status)) {
      Print (L"Failed to install ExitBootServices hook!");
    }
  }

  Status = Root->Close (Root);
  if (EFI_ERROR (Status)) {
    Print (L"Root->Close failed: %llx\n", Status);
    goto Cleanup;
  }

 Cleanup:
  return Status;
}

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

#include <Guid/SmBios.h>
#include <IndustryStandard/SmBios.h>

#include <Guid/Fdt.h>

#include "Common.h"

STATIC struct {
  UINT32 Crc32;
  UINT32 TotalSize;
  VOID   *Data;
} mBlobInfo;

/*
 * These map to what linux prints at boot, when you see a string like:
 *
 *   DMI: LENOVO 81JL/LNVNB161216, BIOS ...
 *
 * We don't really care about the BIOS version information, but the
 * first part gives a reasonable way to pick a dtb.
 */
STATIC struct {
  CHAR16 *SysVendor;    /* System Information/Manufacturer */
  CHAR16 *ProductName;  /* System Information/Product Name */
  CHAR16 *BoardName;    /* Base Board Information/Product Name */
} mSmbiosInfo;

STATIC EFI_EVENT mSmbiosTableEvent;
STATIC EFI_EVENT mSmbios3TableEvent;

// HACK, why do I need this?
EFI_GUID gEfiSmbios3TableGuid = { 0xF2FD1544, 0x9794, 0x4A2C, { 0x99, 0x2E, 0xE5, 0xBB, 0xCF, 0x20, 0xE3, 0x94 }};
EFI_GUID gEfiSmbiosTableGuid = { 0xEB9D2D31, 0x2D88, 0x11D3, { 0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D }};

STATIC
VOID *
GetSmbiosTable (VOID)
{
  SMBIOS_TABLE_ENTRY_POINT     *SmbiosTable = NULL;
  SMBIOS_TABLE_3_0_ENTRY_POINT *Smbios64BitTable = NULL;
  EFI_STATUS                   Status;

  Status = EfiGetSystemConfigurationTable (&gEfiSmbios3TableGuid, (VOID**)&Smbios64BitTable);
  if (Smbios64BitTable) {
    Print (L"Got 64b SMBIOS Table\n");
    return (VOID *) (UINTN) (Smbios64BitTable->TableAddress);
  } else {
    Status = EfiGetSystemConfigurationTable (&gEfiSmbiosTableGuid, (VOID**)&SmbiosTable);
    if (SmbiosTable) {
      Print (L"Got SMBIOS Table\n");
      return (VOID *) (UINTN) (SmbiosTable->TableAddress);
    } else {
      Print (L"%a:%d: Status = %x\n", __func__, __LINE__, Status);
      return NULL;
    }
  }
}


STATIC
CHAR16*
LibGetSmbiosString (
  IN  SMBIOS_STRUCTURE_POINTER    *Smbios,
  IN  UINT16                      StringNumber
  )
{
  UINT16  Index;
  CHAR8   *String;

  ASSERT (Smbios != NULL);

  //
  // Skip over formatted section
  //
  String = (CHAR8 *) (Smbios->Raw + Smbios->Hdr->Length);

  //
  // Look through unformatted section
  //
  for (Index = 1; Index <= StringNumber; Index++) {
    if (StringNumber == Index) {
      UINTN StrSize = AsciiStrnLenS (String, ~0) + 1;
      CHAR16 *String16 = AllocatePool (StrSize * sizeof (CHAR16));
      AsciiStrToUnicodeStrS (String, String16, StrSize);
      return String16;
    }
    //
    // Skip string
    //
    for (; *String != 0; String++);
    String++;

    if (*String == 0) {
      //
      // If double NULL then we are done.
      //  Return pointer to next structure in Smbios.
      //  if you pass in a -1 you will always get here
      //
      Smbios->Raw = (UINT8 *)++String;
      return NULL;
    }
  }

  return NULL;
}

STATIC
EFI_STATUS
ReadSmbiosInfo (VOID)
{
  SMBIOS_TABLE_TYPE1           *Type1Record;
  SMBIOS_TABLE_TYPE2           *Type2Record;
  SMBIOS_STRUCTURE_POINTER     Smbios;

  Smbios.Raw = GetSmbiosTable();

  if (!Smbios.Raw)
    return EFI_NOT_FOUND;

  while (Smbios.Hdr->Type != 127) {
    Dbg (L"Record: %d\n", Smbios.Hdr->Type);

    if (Smbios.Hdr->Type == SMBIOS_TYPE_SYSTEM_INFORMATION) {
      Type1Record = (SMBIOS_TABLE_TYPE1 *) Smbios.Raw;
      mSmbiosInfo.SysVendor = LibGetSmbiosString(&Smbios, Type1Record->Manufacturer);
      mSmbiosInfo.ProductName = LibGetSmbiosString(&Smbios, Type1Record->ProductName);
    }

    if (Smbios.Hdr->Type == SMBIOS_TYPE_BASEBOARD_INFORMATION) {
      Type2Record = (SMBIOS_TABLE_TYPE2 *) Smbios.Raw;
      mSmbiosInfo.BoardName = LibGetSmbiosString(&Smbios, Type2Record->ProductName);
    }

    //
    // Walk to next structure
    //
    LibGetSmbiosString (&Smbios, (UINT16) (-1));
  }

  Dbg (L"SysVendor=%s, ProductName=%s, BoardName=%s\n", mSmbiosInfo.SysVendor, mSmbiosInfo.ProductName, mSmbiosInfo.BoardName);

  return EFI_SUCCESS;
}

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

STATIC
EFI_STATUS
LoadAndRegisterDtb (VOID)
{
  EFI_LOADED_IMAGE_PROTOCOL       *LoadedImage;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
  EFI_FILE_PROTOCOL               *Root;
  EFI_STATUS                      Status;
  VOID                            *Blob;

  Dbg (L"LoadAndRegisterDtb\n");

  Status = GetLoadedImageProtocol (&LoadedImage);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = GetLoadedImageFileSystem (LoadedImage, &FileSystem);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ReadSmbiosInfo();
  if (EFI_ERROR (Status)) {
    Print (L"Failed to read SMBIOS info: Status = %x\n", Status);
    goto Cleanup;
  }

  Status = FileSystem->OpenVolume (FileSystem, &Root);
  if (EFI_ERROR (Status)) {
    Print (L"OpenVolume call failed!\n");
    goto Cleanup;
  }

  /* First try \dtb\$SysVendor\$ProductName-$BoardName.dtb */
  Status = ReadFdt (
      &Blob,
      Root,
      L"\\dtb\\%s\\%s-%s.dtb",
      mSmbiosInfo.SysVendor,
      mSmbiosInfo.ProductName,
      mSmbiosInfo.BoardName
      );
  if (EFI_ERROR (Status)) {
    /* Then fallback to \dtb\$SysVendor\$ProductName.dtb */
    Status = ReadFdt (
        &Blob,
        Root,
        L"\\dtb\\%s\\%s.dtb",
        mSmbiosInfo.SysVendor,
        mSmbiosInfo.ProductName
        );
  }

  if (EFI_ERROR (Status)) {
    /* finally fallback to trying \MY.DTB: */
    // TODO should we try this first, as a convenient way to override default dtb for devel??
    Status = ReadFdt (&Blob, Root, L"\\MY.dtb");
  }
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

STATIC
VOID
OnSmbiosTablesRegistered (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS Status;

  Dbg (L"OnSmbiosTablesRegistered\n");

  if (GetSmbiosTable()) {
    Status = LoadAndRegisterDtb();
    Print (L"%a:%d: Status = %x\n", __func__, __LINE__, Status);
    if (Status == EFI_SUCCESS) {
      gBS->CloseEvent(mSmbiosTableEvent);
      gBS->CloseEvent(mSmbios3TableEvent);
    }
  }
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
  EFI_STATUS Status;

  if (GetSmbiosTable()) {
    /* already got SMBIOS tables configured, so just go: */
    return LoadAndRegisterDtb();
  }

  /*
   * SMBIOS config tables not ready yet, so hook in notifier to do our work
   * later once they are registered:
   */
  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK,
                  OnSmbiosTablesRegistered, NULL, &gEfiSmbios3TableGuid,
                  &mSmbios3TableEvent);
  ASSERT_EFI_ERROR (Status);

  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK,
                  OnSmbiosTablesRegistered, NULL, &gEfiSmbiosTableGuid,
                  &mSmbiosTableEvent);
  ASSERT_EFI_ERROR (Status);

  return Status;
}


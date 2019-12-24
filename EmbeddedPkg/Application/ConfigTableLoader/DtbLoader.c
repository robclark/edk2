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
#include <Guid/FileInfo.h>

#include <Protocol/EsrtManagement.h>
#include <Guid/SystemResourceTable.h>

#include "Common.h"
#include "Qcom.h"

STATIC struct {
  UINT32 Crc32;
  UINT32 FileSize;
  UINT32 TotalSize;
  VOID   *Data;
} mBlobInfo;

SMBIOS_INFO mSmbiosInfo;

STATIC EFI_EVENT mSmbiosTableEvent;
STATIC EFI_EVENT mSmbios3TableEvent;

/* strawman: use version #'s that match linux kernel from which the dtb
 * files came?  Alternatively we could use 3 components, like:
 *
 *   $dtbver . $kernel_major . $kernel_minor
 *
 * so that updates to DtbLoader itself take precedence?
 *
 * XXX actually fwupdmgr shows version as 0.5.4, so I guess it is interpreting
 * as XX.YY.ZZZZ...
 */
#define DTB_LOADER_VERSION(major, minor) (((major) << 16) | (minor))

STATIC EFI_SYSTEM_RESOURCE_ENTRY mSystemResourceEntry = {
    .FwClass = {0x45eaa15e, 0x0160, 0x4dc0, {0xb2, 0x88, 0xc9, 0x61, 0xdf, 0x9c, 0x62, 0x65}},
    .FwType = ESRT_FW_TYPE_UEFIDRIVER,
    .FwVersion = DTB_LOADER_VERSION(5, 4),
    .LowestSupportedFwVersion = 0, // XXX
    .CapsuleFlags = 0, // XXX
};

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

#define FDT_ADDITIONAL_SIZE 0x400

/* Increase the size of the FDT blob so that we can patch in new nodes */
STATIC
EFI_STATUS
ResizeBlob (
  IN OUT VOID **Blob
  )
{
  VOID  *NewBlob;
  UINTN NewSize;
  INTN  Err;

  NewSize = fdt_totalsize (*Blob) + FDT_ADDITIONAL_SIZE;
  NewBlob = AllocatePool (NewSize);
  if (!NewBlob) {
    Print (L"%a:%d: allocation failed\n", __func__, __LINE__);
    return EFI_OUT_OF_RESOURCES;
  }

  Err = fdt_open_into (*Blob, NewBlob, NewSize);
  if (Err) {
    Print (L"Could not expand fdt: %a\n", fdt_strerror (Err));
    FreePool (NewBlob);
    return EFI_OUT_OF_RESOURCES;
  }

  /* Successfully Resized: */
  mBlobInfo.FileSize += FDT_ADDITIONAL_SIZE;
  mBlobInfo.Data = NewBlob;

  FreePool (*Blob);
  *Blob = NewBlob;

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ReadBlob (
  IN     EFI_FILE_PROTOCOL *Root,
  IN     CHAR16            *Name,
  IN OUT VOID              **Blob
  )
{
  EFI_FILE_PROTOCOL *File;
  EFI_STATUS        Status;
  UINTN             BufferSize;
  EFI_FILE_INFO     *FileInfo;

  Status = Root->Open (Root, &File, Name, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to open '%s'\n", Name);
    return Status;
  }

  Print (L"File '%s' opened successfully!\n", Name);
  BufferSize = 0;
  Status = File->GetInfo (File, &gEfiFileInfoGuid, &BufferSize, NULL);

  FileInfo = AllocatePool (BufferSize);
  if (!FileInfo) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Cleanup;
  }

  Status = File->GetInfo (File, &gEfiFileInfoGuid, &BufferSize, FileInfo);
  Print (L"File size: %d bytes\n", FileInfo->FileSize);

  // Don't bother loading the file if it's smaller than the DT header
  if (FileInfo->FileSize < sizeof (struct fdt_header)) {
    Print (L"'%s' is not a valid .dtb (too small) - not using!\n", Name);
    Status = EFI_INVALID_PARAMETER;
    goto Cleanup;
  }

  *Blob = AllocatePool (FileInfo->FileSize);
  if (!*Blob) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Cleanup;
  }

  Status = File->Read (File, &FileInfo->FileSize, *Blob);
  if (EFI_ERROR (Status)) {
    FreePool (*Blob);
    goto Cleanup;
  }

  // Save DT info to detect changes.
  mBlobInfo.FileSize = FileInfo->FileSize;
  mBlobInfo.Data = *Blob;

 Cleanup:
  FreePool (FileInfo);
  Root->Close (File);
  return Status;
}

/**
  Construct a path string from an array of path components, and try to load
  a dtb blob from that path.

  The file path is constructed as (for example):

     PathComponents[0] + "\" + ... + "\" + PathComponents[n] + "\" +
     FileComponents[0] + "-" + ... + "-" + FileComponents[m] + ".dtb"

**/
EFI_STATUS
TryLoadBlob (
  IN     EFI_FILE_PROTOCOL *Root,
  IN     CHAR16            **PathComponents,
  IN     UINT32            PathComponentsLen,
  IN     CHAR16            **FileComponents,
  IN     UINT32            FileComponentsLen,
  IN OUT VOID              **Blob
  )
{
  CHAR16                          *BlobName;
  UINTN                           BlobPathLength;
  EFI_STATUS                      Status;

  ASSERT (PathComponentsLen > 0);
  ASSERT (FileComponentsLen > 0);

  BlobPathLength = 1;  // terminating null

  for (UINT32 i = 0; i < PathComponentsLen; i++) {
    BlobPathLength += StrLen (PathComponents[i]);
    BlobPathLength += 1; // +1 for '\'
  }

  for (UINT32 i = 0; i < FileComponentsLen; i++) {
    BlobPathLength += StrLen (FileComponents[i]);
    BlobPathLength += 1; // +1 for '\' or '.'
  }

  BlobPathLength += 3;  // for "dtb"

  BlobName = AllocatePool (BlobPathLength * sizeof (CHAR16));
  if (BlobName == NULL) {
    Print (L"Memory allocation failed!\n");
    return EFI_OUT_OF_RESOURCES;
  }

  BlobName[0] = L'\0';

  for (UINT32 i = 0; i < PathComponentsLen; i++) {
    Status = StrCatS (BlobName, BlobPathLength, PathComponents[i]);
    if (EFI_ERROR (Status)) {
      Print (L"%a:%d: Status = %x\n", __func__, __LINE__, Status);
      goto Cleanup;
    }
    Status = StrCatS (BlobName, BlobPathLength, L"\\");
    if (EFI_ERROR (Status)) {
      Print (L"%a:%d: Status = %x\n", __func__, __LINE__, Status);
      goto Cleanup;
    }
  }

  for (UINT32 i = 0; i < FileComponentsLen; i++) {
    if (i > 0) {
      Status = StrCatS (BlobName, BlobPathLength, L"-");
      if (EFI_ERROR (Status)) {
        Print (L"%a:%d: Status = %x\n", __func__, __LINE__, Status);
        goto Cleanup;
      }
    }
    Status = StrCatS (BlobName, BlobPathLength, FileComponents[i]);
    if (EFI_ERROR (Status)) {
      Print (L"%a:%d: Status = %x\n", __func__, __LINE__, Status);
      goto Cleanup;
    }
  }

  Status = StrCatS (BlobName, BlobPathLength, L".dtb");
  if (EFI_ERROR (Status)) {
    Print (L"%a:%d: Status = %x\n", __func__, __LINE__, Status);
    goto Cleanup;
  }

  Dbg (L"Try to load: %s\n", BlobName);

  Status = ReadBlob (Root, BlobName, Blob);

 Cleanup:
  FreePool (BlobName);
  return Status;
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
  gBS->CalculateCrc32 (mBlobInfo.Data, mBlobInfo.FileSize, &mBlobInfo.Crc32);
  mBlobInfo.TotalSize = fdt_totalsize (mBlobInfo.Data);

  Print (L"DT CRC32: %08x\n", mBlobInfo.Crc32);
  Print (L"DT TotalSize: %d bytes\n", mBlobInfo.TotalSize);
  Print (L"DT FileSize: %d bytes\n", mBlobInfo.FileSize);
  if (mBlobInfo.FileSize < mBlobInfo.TotalSize) {
    Print (L"Warning: File size (%d bytes) < TotalSize\n", mBlobInfo.FileSize);
  }

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
  Status = TryLoadBlob (
      Root,
      (CHAR16 *[]){ L"\\dtb", mSmbiosInfo.SysVendor }, 2,
      (CHAR16 *[]){ mSmbiosInfo.ProductName, mSmbiosInfo.BoardName }, 2,
      &Blob
      );
  if (EFI_ERROR (Status)) {
    /* Then fallback to \dtb\$SysVendor\$ProductName.dtb */
    Status = TryLoadBlob (
        Root,
        (CHAR16 *[]){ L"\\dtb", mSmbiosInfo.SysVendor }, 2,
        (CHAR16 *[]){ mSmbiosInfo.ProductName }, 1,
        &Blob
        );
  }

  if (EFI_ERROR (Status)) {
    /* finally fallback to trying \MY.DTB: */
    // TODO should we try this first, as a convenient way to override default dtb for devel??
    Status = TryLoadBlob (
        Root,
        (CHAR16 *[]){ L"" }, 1,
        (CHAR16 *[]){ L"MY" }, 1,
        &Blob
        );
  }

  if (!EFI_ERROR (Status)) {
    EFI_EVENT ExitBootServicesEvent;

    ResizeBlob (&Blob);
    QcomDetectPanel (Root, Blob);
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

STATIC
EFI_STATUS
UpdateEsrtEntry (VOID)
{
  ESRT_MANAGEMENT_PROTOCOL *EsrtManagement;
  EFI_STATUS Status;

  Dbg (L"Locate Protocol\n");

  Status = gBS->LocateProtocol(&gEsrtManagementProtocolGuid, NULL, (VOID **)&EsrtManagement);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to locate ESRT_MANAGEMENT_PROTOCOL! %llx\n", Status);
    return Status;
  }

  Dbg (L"Register ESRT\n");

  /* RegisterEsrtEntry() doesn't seem to do anything if there is already
   * an entry installed.. and UpdateEsrtEntry() doesn't seem to do anything
   * if there *isn't* already an entry.. so maybe we have to do both?
   */
  Status = EsrtManagement->RegisterEsrtEntry(&mSystemResourceEntry);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to register EFI_SYSTEM_RESOURCE_ENTRY! %llx\n", Status);
    return Status;
  }

  Dbg (L"Update ESRT\n");

  Status = EsrtManagement->UpdateEsrtEntry(&mSystemResourceEntry);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to register EFI_SYSTEM_RESOURCE_ENTRY! %llx\n", Status);
    return Status;
  }

  return Status;
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

  UpdateEsrtEntry();

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


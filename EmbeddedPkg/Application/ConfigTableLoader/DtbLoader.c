/** @file
  Application to load and register a .dtb file.

  Replaces any existing registration.

  Copyright (c) 2019, Linaro. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <libfdt.h>
#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Guid/Fdt.h>
#include <Guid/FileInfo.h>

#include "Common.h"

#define DTB_BLOB_NAME L"MY.DTB"

STATIC struct {
  UINT32 Crc32;
  UINT32 FileSize;
  UINT32 TotalSize;
  VOID   *Data;
} mBlobInfo;

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
  gBS->CalculateCrc32 (*Blob, FileInfo->FileSize, &mBlobInfo.Crc32);
  mBlobInfo.FileSize = FileInfo->FileSize;
  mBlobInfo.TotalSize = fdt_totalsize (*Blob);
  mBlobInfo.Data = *Blob;

  Print (L"DT CRC32: %08x\n", mBlobInfo.Crc32);
  Print (L"DT TotalSize: %d bytes\n", mBlobInfo.TotalSize);
  if (mBlobInfo.FileSize < mBlobInfo.TotalSize) {
    Print (L"Warning: File size (%d bytes) < TotalSize\n", mBlobInfo.FileSize);
  }

 Cleanup:
  FreePool (FileInfo);
  Root->Close (File);
  return Status;
}

STATIC
EFI_STATUS
RegisterDtBlob (
  IN VOID *Blob
  )
{
  EFI_STATUS Status;

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
  EFI_DEVICE_PATH_PROTOCOL        *ImagePath;
  EFI_FILE_PROTOCOL               *Root;
  CHAR16                          *FileName;
  CHAR16                          *TempString;
  CHAR16                          *BlobName;
  UINTN                           BlobPathLength;
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

  ImagePath = LoadedImage->FilePath;
  FileName = (CHAR16 *)((UINTN)ImagePath + 4);

  // Even paths to files in root directory start with '\'
  TempString = StrRChr (FileName, L'\\');
  if (TempString == NULL) {
    Print (L"Invalid path for image: '%s'\n", FileName);
    return EFI_UNSUPPORTED;
  }

  BlobPathLength = StrLen (DTB_BLOB_NAME);
  BlobPathLength += TempString - FileName + 1; // + 1 for '\'
  BlobPathLength += 1; // + 1 for '\0'

  BlobName = AllocatePool (BlobPathLength * sizeof (CHAR16));
  if (BlobName == NULL) {
    Print (L"Memory allocation failed!\n");
    return EFI_OUT_OF_RESOURCES;
  }

  Status = StrnCpyS (BlobName, BlobPathLength, FileName,
		     TempString - FileName + 1);
  if (EFI_ERROR (Status)) {
    Print (L"Status = %x\n", Status);
  }
  Status = StrCatS (BlobName, BlobPathLength, DTB_BLOB_NAME);
  if (EFI_ERROR (Status)) {
    Print (L"Status = %x\n", Status);
  }

  Status = FileSystem->OpenVolume (FileSystem, &Root);
  if (EFI_ERROR (Status)) {
    Print (L"OpenVolume call failed!\n");
    goto Cleanup;
  }

  Status = ReadBlob (Root, BlobName, &Blob);
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
  FreePool (BlobName);

  return Status;
}

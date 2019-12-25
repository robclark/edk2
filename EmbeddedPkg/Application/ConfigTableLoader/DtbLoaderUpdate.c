/** @file
  Application to update DtbLoader and dtb files

  Copyright (c) 2019, Linaro. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Guid/SmBios.h>
#include <IndustryStandard/SmBios.h>

#include <Guid/FileInfo.h>

#include <Protocol/EsrtManagement.h>
#include <Guid/SystemResourceTable.h>

#include "Common.h"

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
  EFI_FILE_PROTOCOL               *File = NULL;
  CHAR16                          *Name = L"\\somefile.txt";
  EFI_STATUS                      Status;
  UINTN                           BufferSize;
  VOID                            *Blob = L"some string";

  Dbg (L"Update DtbLoader\n");

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

  Status = Root->Open (
      Root,
      &File,
      Name,
      EFI_FILE_MODE_CREATE | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ,
      0
      );
  if (EFI_ERROR (Status)) {
    Print (L"Failed to open '%s'\n", Name);
    return Status;
  }

  Print (L"File '%s' opened successfully!\n", Name);

  BufferSize = StrnLenS(Blob, 999);
  Status = File->Write (File, &BufferSize, Blob);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to write '%s'\n", Name);
    goto Cleanup;
  }


 Cleanup:
  Root->Close (File);
  return Status;
}


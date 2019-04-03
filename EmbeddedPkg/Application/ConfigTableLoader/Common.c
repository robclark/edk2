/** @file

  Copyright (c) 2019, Linaro. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/LoadedImage.h>

/**
  Acquires a LOADED_IMAGE_PROTOCOL structure that points to the instance
  for the currently executing image.

  @param[in][out] LoadedImage  Pointer to EFI_LOADED_IMAGE_PROTOCOL pointer.

  @retval EFI_SUCCESS          The structure was located successfully.
  @retval other                The return value from gBS->OpenProtocol call.

**/
EFI_STATUS
EFIAPI
GetLoadedImageProtocol (
  IN OUT EFI_LOADED_IMAGE_PROTOCOL **LoadedImage
  )
{
  EFI_STATUS Status;

  Status = gBS->OpenProtocol (gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **) LoadedImage,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  if (EFI_ERROR (Status)) {
    Print (L"Failed to open EFI_LOADED_IMAGE_PROTOCOL! %llx\n", Status);
  }

  return Status;
}

/**
  Acquires a SIMPLE_FILE_SYSTEM_PROTOCOL instance for the device handle of
  the supplied   EFI_LOADED_IMAGE_PROTOCOL instance.

  @param[in]      LoadedImage  Pointer to EFI_LOADED_IMAGE_PROTOCOL structure.
  @param[in][out] FileSystem   Pointer to EFI_SIMPLE_FILE_SYSTEM_PROTOCOL pointer.

  @retval EFI_SUCCESS          The structure was located successfully.
  @retval other                The return value from gBS->OpenProtocol call.

**/
EFI_STATUS
EFIAPI
GetLoadedImageFileSystem (
  IN     EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  IN OUT EFI_SIMPLE_FILE_SYSTEM_PROTOCOL **FileSystem
  )
{
  EFI_STATUS Status;

  Status = gBS->OpenProtocol (LoadedImage->DeviceHandle,
			      &gEfiSimpleFileSystemProtocolGuid,
			      (VOID **)FileSystem,
			      gImageHandle,
			      NULL,
			      EFI_OPEN_PROTOCOL_GET_PROTOCOL
			      );
  if (EFI_ERROR (Status)) {
    Print (L"Failed to open SIMPLE_FILE_SYSTEM_PROTOCOL! %llx\n", Status);
  }

  return Status;
}

CHAR16 *
EFIAPI
StrRChr (
  IN CHAR16 *String,
  IN CHAR16 Character
  )
{
  CHAR16 *Pos;

  Pos = String + StrLen (String) - 1;

  while (Pos-- > String) {
    if (*Pos == Character) {
      return Pos;
    }
  }

  return NULL;
}

/** @file

  Copyright (c) 2019, Linaro. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef COMMON_H_
#define COMMON_H_

#include <Uefi.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

/**
  Acquires a LOADED_IMAGE_PROTOCOL structure that points to the instance
  for the currently executing image.

  @param[in][out] LoadedImage  Pointer to LOADED_IMAGE_PROTOCOL pointer.

  @retval EFI_SUCCESS          The structure was located successfully.
  @retval other                The return value from gBS->OpenProtocol call.

**/
EFI_STATUS
EFIAPI
GetLoadedImageProtocol (
  IN OUT EFI_LOADED_IMAGE_PROTOCOL **LoadedImage
  );

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
  );

CHAR16 *
EFIAPI
StrRChr (
 IN CHAR16 *String,
 IN CHAR16 Character
 );

#endif /* COMMON_H_ */

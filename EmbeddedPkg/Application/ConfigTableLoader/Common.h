/** @file

  Copyright (c) 2019, Linaro. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef COMMON_H_
#define COMMON_H_

#include <Uefi.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

#define Dbg(...) do { Print(__VA_ARGS__); gBS->Stall(100000); } while (0)

/*
 * These map to what linux prints at boot, when you see a string like:
 *
 *   DMI: LENOVO 81JL/LNVNB161216, BIOS ...
 *
 * We don't really care about the BIOS version information, but the
 * first part gives a reasonable way to pick a dtb.
 */
typedef struct {
  CHAR16 *SysVendor;    /* System Information/Manufacturer */
  CHAR16 *ProductName;  /* System Information/Product Name */
  CHAR16 *BoardName;    /* Base Board Information/Product Name */
} SMBIOS_INFO;

extern SMBIOS_INFO mSmbiosInfo;

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
  );

#endif /* COMMON_H_ */

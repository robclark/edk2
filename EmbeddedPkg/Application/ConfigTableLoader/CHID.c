/** @file
  Application to load and register a .dtb file.

  Replaces any existing registration.

  Copyright (c) 2019, Linaro. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Guid/SmBios.h>
#include <IndustryStandard/SmBios.h>

#include "Common.h"

SMBIOS_INFO mSmbiosInfo;

// HACK, why do I need this?
EFI_GUID gEfiSmbios3TableGuid = { 0xF2FD1544, 0x9794, 0x4A2C, { 0x99, 0x2E, 0xE5, 0xBB, 0xCF, 0x20, 0xE3, 0x94 }};
EFI_GUID gEfiSmbiosTableGuid = { 0xEB9D2D31, 0x2D88, 0x11D3, { 0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D }};

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


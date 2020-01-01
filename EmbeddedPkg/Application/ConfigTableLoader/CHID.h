/** @file

  Copyright (c) 2019, Linaro. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CHID_H_
#define CHID_H_

#include <Uefi.h>

/**
  The different "ComputerHardwareIds" match microsoft and fwupd[1].  The
  CHID/UUID used to try to pick a dtb file are generated according to the
  fields specified in comment (using L"&" as separator character) below.

  (Not all possible CHIDs are supported, although once pulling enough
  fields out of SMBIOS, it should be straightforward to add more if need
  arises.)

  [1] https://blogs.gnome.org/hughsie/2017/04/25/reverse-engineering-computerhardwareids-exe-with-winedbg/
 */
typedef enum {
  CHID_0,      // Manufacturer + Family + ProductName + ProductSku + BiosVendor + BiosVersion + BiosMajorRelease + BiosMinorRelease
  CHID_1,      // Manufacturer + Family + ProductName + BiosVendor + BiosVersion + BiosMajorRelease + BiosMinorRelease
  CHID_2,      // Manufacturer + ProductName + BiosVendor + BiosVersion + BiosMajorRelease + BiosMinorRelease
  CHID_3,      // Manufacturer + Family + ProductName + ProductSku + BaseboardManufacturer + BaseboardProduct
  CHID_4,      // Manufacturer + Family + ProductName + ProductSku
  CHID_5,      // Manufacturer + Family + ProductName
  CHID_6,      // Manufacturer + ProductSku + BaseboardManufacturer + BaseboardProduct
  CHID_7,      // Manufacturer + ProductSku
  CHID_8,      // Manufacturer + ProductName + BaseboardManufacturer + BaseboardProduct
  CHID_9,      // Manufacturer + ProductName
  CHID_10,     // Manufacturer + Family + BaseboardManufacturer + BaseboardProduct
  CHID_11,     // Manufacturer + Family
  CHID_12,     // Manufacturer + EnclosureKind
  CHID_13,     // Manufacturer + BaseboardManufacturer + BaseboardProduct
  CHID_14,     // Manufacturer
} CHID;

VOID *
GetSmbiosTable (VOID);

EFI_STATUS
ReadSmbiosInfo (VOID);

#endif /* CHID_H_ */

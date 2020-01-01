/** @file

  Copyright (c) 2019, Linaro. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef QCOM_H_
#define QCOM_H_

#include <Uefi.h>

#include "CHID.h"

/**
  Detect (if present) the qcom specific UEFIDisplayInfo variable, adjust dtb
  accordingly

  @param[in] Root         The root of filesys that Blob was loaded from (ie.
                          where to look for overlays)
  @param[in] Blob         The fdt blob to patch if panel id is detected
  @param[in] CurrentCHID  The CHID used to construct path that blob was loaded
                          from

  @retval EFI_SUCCESS          The panel was detected and dt patched successfully.
  @retval other                Error.
**/
EFI_STATUS
EFIAPI
QcomDetectPanel (
  IN EFI_FILE_PROTOCOL *Root,
  IN VOID              *Blob,
  IN CHID              CurrentCHID
  );

#endif /* QCOM_H_ */

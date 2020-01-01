/** @file

  Copyright (c) 2019, Linaro. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef QCOM_H_
#define QCOM_H_

#include <Uefi.h>

/**
  Detect (if present) the qcom specific UEFIDisplayInfo variable, and
  patch /chosen/panel-id accordingly.

  @param[in] Blob  The fdt blob to patch if panel id is detected

  @retval EFI_SUCCESS          The panel was detected and dt patched successfully.
  @retval other                Error.

**/
EFI_STATUS
EFIAPI
QcomDetectPanel (
  IN EFI_FILE_PROTOCOL *Root,
  IN VOID              *Blob
  );

#endif /* QCOM_H_ */

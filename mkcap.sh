#!/bin/bash

set -ex

BASE_TOOLS_PATH=./BaseTools
GUID=45eaa15e-0160-4dc0-b288-c961df9c6265
VERSION=0x00050005
BIN=./Build/Embedded/DEBUG_GCC5/AARCH64/DtbLoaderUpdate.efi

./BaseTools/BinWrappers/PosixLike/GenerateCapsule \
	-v \
	--encode \
	--guid $GUID \
	--fw-version $VERSION \
	--lsv 0x00000000 \
	--capflag PersistAcrossReset \
	--capflag InitiateReset \
	--signer-private-cert=$BASE_TOOLS_PATH/Source/Python/Pkcs7Sign/TestCert.pem \
	--other-public-cert=$BASE_TOOLS_PATH/Source/Python/Pkcs7Sign/TestSub.pub.pem \
	--trusted-public-cert=$BASE_TOOLS_PATH/Source/Python/Pkcs7Sign/TestRoot.pub.pem \
	-o DtbLoaderUpdate.cap \
	$BIN

cat > tmp.metainfo.xml <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<component type="firmware">
  <id>org.linaro.DtbLoader</id>
  <name>DtbLoader</name>
  <summary>DTB Table Loader</summary>
  <description>
    Updated Device Tree config table loader for windows aarch64 laptops.
  </description>
  <provides>
    <firmware type="flashed">$GUID</firmware>
  </provides>
  <url type="homepage">https://github.com/aarch64-laptops/build</url>
  <metadata_license>CC0-1.0</metadata_license>
  <project_license>BSD</project_license>
  <developer_name>Linaro</developer_name>
  <releases>
    <release version="$VERSION" date="`date`">
      <description>
        Build $VERSION
      </description>
    </release>
  </releases>
  <!-- most OEMs do not need to do this... -->
  <custom>
    <value key="LVFS::InhibitDownload"/>
  </custom>
</component>
EOF

gcab --create DtbLoaderUpdate.cab DtbLoaderUpdate.cap tmp.metainfo.xml

rm tmp.metainfo.xml

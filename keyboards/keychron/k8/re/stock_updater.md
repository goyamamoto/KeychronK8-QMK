# Keychron's K8 RGB v1.07 updaters

Keychron's K8 firmware page (keychron.com/pages/firmware-for-keychron-k8)
offers the RGB v1.07 update, labelled `K8-A2-87K-RGB-V1.07`, as a Windows
`.exe` and a macOS `.zip`. Both flash `05AC:024F` (ANSI RGB) through the
SN32F24xB ISP protocol. SHA-256 of the files described here:

    6924a8dbf52f01acd001e3dac6aa1f91473834e99b04f3ecd0a5723d671ee3bc  K8-A2-87K-RGB-V1.07.exe
    90e17ecd4e116bd8dfcb70721f3d52a194a32eff2ec0a89cf2904b368c56267a  K8-A2-87K-RGB-V1.07.zip

## Windows updater: plain image in resources

`K8-A2-87K-RGB-V1.07.exe` is a 32-bit MFC program titled "HFD ISP Tool". Its
`RT_RCDATA` resources hold everything it flashes, unencrypted:

| ID | Content |
|---|---|
| 4000 | The 64 KiB flash image (vector table SP `0x20001738`, reset `0x5f89`; USB descriptor `05AC:024F`, bcdDevice `0107`, "Keychron K8") |
| 4007 | `12345678`, UTF-16 (the ISP password, logged as "get pwd is ...") |
| 4008 | `05AC`, UTF-16 (target VID) |
| 4009 | `024F`, UTF-16 (target PID) |
| 4010 | `SN32F24xB`, UTF-16 (chip) |
| 4011 | A 73137-byte ZIP of UI settings (see below) |
| 4012 | One byte, `0x01` |
| 4013 | `Build time:2020/12/17 21:47`, UTF-16 |
| 4014 | `0B76`, UTF-16: the 16-bit little-endian word sum of the image in 4000 |

[`extract_official.py`](extract_official.py) takes 4000 and checks it against
4008, 4009 and 4014. The sum is the same one SonixFlasherC computes, so a
flash of this image verifies against `0B76`.

The ZIP in 4011 (no encryption) holds `Settings/UISettings*.ini`, bitmaps and
`Settings/FWFiles/SN32F260.hex`. That hex is not the K8 image: its descriptor
is `05AC:0256` bcdDevice `0109`, left over from another product.
`UISettings_248B.ini` has:

- `CheckDeviceCmd=AA42895AFF7162CC`: the eight bytes the updater sends to
  stock firmware to enter ISP mode. As two little-endian words they are
  `5A8942AA CC6271FF`, the "hfd" reboot command of SonixFlasherC.
- `[SpecialPID1] VID=0x05AC PID=0x024F ChipName=SN32F24xB` with
  `FWName=SN32F240B_32.hex` and `CodeSecurity=1`: the updater flashes the K8
  under its stock USB ID and leaves code security at CS1.

## macOS updater: encrypted image

`K8-A2-87K-RGB-V1.07.app` is Sonix's `ISPCTool` 1.0.12
(`com.sw.sonix.ISPCTool`), an x86_64, sandboxed app.
`Contents/Resources/UserInfo.plist` carries:

| Key | Value |
|---|---|
| `ISPDeviceInfo` | `05ac:024f` |
| `ISPChipName` | `SN32F24xB` |
| `ISPCSNumber` | 1 |
| `ISPCheckSumDic.data` | `AA 42 89 5A FF 71 62 CC`, the same reboot command as `CheckDeviceCmd` |
| `ISPFWFileName` | `K8-A2-87K-RGB-V1.07_0x0B76.hex` |
| `ISPToken` | empty string |
| `ISPFWData` | 50768 bytes, `compressed` = 1, `ratio` 3.02 |
| `ISPTitleIcon` | LZFSE (`bvx2`) data |

On launch, `unZipFWDataWithFileName:token:` passes `ISPFWData` to
`decryptData:withAlgorithm:password:` with algorithm 0 (AES-128),
`kCCOptionECBMode`, no IV, and the first 16 bytes of the token's UTF-8 string
as the key, then LZFSE-decompresses the result into an Intel HEX file in the
app container's `Library/Caches`. With the empty token the key is whatever
follows the empty string in memory; all-zero, `12345678`-based and
`CheckDeviceCmd`-based keys do not decrypt it. On macOS 26 under Rosetta the
app logs `Decompress Error` for the 50768-byte data at launch and no hex file
appears in its cache. The image has not been recovered from this updater; the
Windows one gives it directly.

## Stock firmware side

Stock K8 RGB firmware accepts the reboot command: the words `5A8942AA` and
`CC6271FF` sit at `0x155c` in ANSI v1.06 (`0x15ec` in JIS v1.07). After
SonixFlasherC sends it (`-r hfd`, as a 64-byte feature report on interface 0),
the keyboard answers the ISP protocol while staying enumerated as
`05AC:024F`, reports code security CS1, and takes a full flash. On the tested
keyboard the flash word sum before QMK was `0C87`, not `0B76`, most likely
because stock firmware keeps settings in flash. Flashing the stock image back
gives the v1.07 program, not a byte copy of what the keyboard held; that
content was never read out (CS1, and the flasher has no read command).

/** @file
  Courbet probe - prints what the firmware managed to bring up, without input.

  Why this exists
  ---------------
  The SimpleInit menu draws, but the volume and power buttons do nothing, so
  nothing can be selected and no diagnostics can be typed into the shell. This
  driver runs at ReadyToBoot, before SimpleInit takes the screen, prints the
  state of the chains that matter, and holds the screen long enough to read or
  photograph it.

  What it answers
  ---------------
  - whether UFS enumerated: a BlockIo device of the phone's size means the
    storage chain is up;
  - where the PMIC chain stopped: PmicDxe, which feeds the buttons, requires
    the SCM and SPMI-bus protocols, and ButtonsDxe requires two PMIC protocols
    that only PmicDxe produces. Printing each of those separately identifies
    the link that is missing instead of guessing;
  - how many drivers actually dispatched, as a single number.
**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/LoadedImage.h>
#include <Guid/EventGroup.h>

#define HOLD_SECONDS 30

//
// The vendor protocols involved in the button chain. None of them has a
// header in this tree, so they are declared here by value. The names are the
// ones the Qualcomm UEFI code uses; the GUID is what matters.
//
STATIC EFI_GUID  mScmGuid =
  { 0xae37b942, 0x457f, 0x4c91, { 0xa1, 0x96, 0xd9, 0x66, 0x9f, 0xd3, 0x47, 0xa3 } };
STATIC EFI_GUID  mSpmbusGuid =
  { 0xfa5f306b, 0xf47d, 0x4ac4, { 0xa4, 0x7d, 0x88, 0x2f, 0x82, 0x04, 0xec, 0x30 } };
STATIC EFI_GUID  mPmicButtonsGuid =
  { 0x60759b13, 0xa8bf, 0x46fe, { 0xb7, 0xe6, 0x79, 0x7b, 0xfb, 0x33, 0x5d, 0xf3 } };
STATIC EFI_GUID  mPmicChargerGuid =
  { 0x9ba45b66, 0xefa4, 0x441c, { 0xa3, 0xe4, 0xed, 0x22, 0x24, 0x78, 0x6b, 0xe2 } };
STATIC EFI_GUID  mPlatformInfoGuid =
  { 0x157a5c45, 0x21b2, 0x43c5, { 0xba, 0x7c, 0x82, 0x2f, 0xee, 0x5f, 0xe5, 0x99 } };

STATIC
VOID
PrintPresence (
  IN EFI_GUID      *Guid,
  IN CONST CHAR16  *Name
  )
{
  EFI_HANDLE  *Handles;
  UINTN       Count;
  EFI_STATUS  Status;

  Handles = NULL;
  Count   = 0;
  Status  = gBS->LocateHandleBuffer (ByProtocol, Guid, NULL, &Count, &Handles);
  if (!EFI_ERROR (Status) && (Count > 0)) {
    Print (L"  %-28s %u\r\n", Name, (UINT32)Count);
  } else {
    Print (L"  %-28s ABSENT\r\n", Name);
  }

  if (Handles != NULL) {
    FreePool (Handles);
  }
}

STATIC
VOID
PrintBlockDevices (
  VOID
  )
{
  EFI_HANDLE            *Handles;
  UINTN                 Count;
  UINTN                 Index;
  EFI_STATUS            Status;
  EFI_BLOCK_IO_PROTOCOL *BlockIo;
  UINT64                Bytes;

  Handles = NULL;
  Count   = 0;
  Status  = gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &Count, &Handles);
  if (EFI_ERROR (Status) || (Count == 0) || (Handles == NULL)) {
    Print (L"  no BlockIo handle at all\r\n");
    return;
  }

  Print (L"  %u block device(s):\r\n", (UINT32)Count);
  for (Index = 0; Index < Count; Index++) {
    Status = gBS->HandleProtocol (Handles[Index], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
    if (EFI_ERROR (Status) || (BlockIo == NULL) || (BlockIo->Media == NULL)) {
      continue;
    }

    Bytes = (BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;
    Print (
      L"  [%2u] %lu MB  %lu byte blocks  reads=%d writes=%d\r\n",
      (UINT32)Index,
      (UINT32)(Bytes / (1024 * 1024)),
      (UINT32)BlockIo->Media->BlockSize,
      BlockIo->Media->MediaPresent,
      BlockIo->Media->ReadOnly
      );
  }

  FreePool (Handles);
}

STATIC
VOID
EFIAPI
CourbetProbeReadyToBoot (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  Print (L"\r\n");
  Print (L"========================================================\r\n");
  Print (L"  COURBET PROBE - no input needed\r\n");
  Print (L"========================================================\r\n");

  Print (L"\r\nSTORAGE\r\n");
  PrintBlockDevices ();
  PrintPresence ((EFI_GUID *)&gEfiSimpleFileSystemProtocolGuid, L"SimpleFileSystem (FAT)");

  Print (L"\r\nBUTTON CHAIN\r\n");
  PrintPresence (&mScmGuid,      L"SCM        (ae37b942)");
  PrintPresence (&mSpmbusGuid,   L"SPMbus     (fa5f306b)");
  PrintPresence (&mPlatformInfoGuid, L"PlatformInfo (157a5c45)");
  PrintPresence (&mPmicButtonsGuid,  L"PMIC buttons (60759b13)");
  PrintPresence (&mPmicChargerGuid,  L"PMIC charger (9ba45b66)");
  PrintPresence ((EFI_GUID *)&gEfiSimpleTextInProtocolGuid, L"SimpleTextIn");

  Print (L"\r\nDRIVERS / CONSOLES\r\n");
  PrintPresence ((EFI_GUID *)&gEfiLoadedImageProtocolGuid, L"loaded images");

  Print (L"\r\n--------------------------------------------------------\r\n");
  Print (L"  A device of the phone's size above means UFS is up.\r\n");
  Print (L"  PMIC buttons and PMIC charger present means PmicDxe ran,\r\n");
  Print (L"  so a dead button would be a ButtonsDxe or UI problem.\r\n");
  Print (L"  SCM absent with everything else absent points at ScmDxe.\r\n");
  Print (L"\r\nSimpleInit in %d seconds.\r\n", HOLD_SECONDS);

  gBS->Stall (HOLD_SECONDS * 1000000);
}

EFI_STATUS
EFIAPI
CourbetProbeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_EVENT   ReadyToBoot;
  EFI_STATUS  Status;

  ReadyToBoot = NULL;

  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  CourbetProbeReadyToBoot,
                  NULL,
                  &ReadyToBoot
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->RegisterProtocolNotify (
                  &gEfiEventReadyToBootGuid,
                  ReadyToBoot,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    gBS->CloseEvent (ReadyToBoot);
  }

  return Status;
}

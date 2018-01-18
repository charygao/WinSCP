//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include <shlobj.h>
#include <Common.h>

#include "GUITools.h"
#include "GUIConfiguration.h"
#include <TextsCore.h>
#include <CoreMain.h>
#include <SessionData.h>
#include <WinInterface.h>
#include <TbxUtils.hpp>
#include <Math.hpp>
#include <WebBrowserEx.hpp>
#include <Tools.h>
#include "PngImageList.hpp"
#include <StrUtils.hpp>
#include <limits>
#include <Glyphs.h>
#include <PasTools.hpp>
#include <VCLCommon.h>
#include <Vcl.ScreenTips.hpp>

#include "Animations96.h"
#include "Animations120.h"
#include "Animations144.h"
#include "Animations192.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
extern const UnicodeString PageantTool = L"pageant.exe";
extern const UnicodeString PuttygenTool = L"puttygen.exe";
//---------------------------------------------------------------------------
bool __fastcall FindFile(UnicodeString & Path)
{
  bool Result = FileExists(ApiPath(Path));

  if (!Result)
  {
    UnicodeString ProgramFiles32 = IncludeTrailingBackslash(GetEnvironmentVariable(L"ProgramFiles"));
    UnicodeString ProgramFiles64 = IncludeTrailingBackslash(GetEnvironmentVariable(L"ProgramW6432"));
    if (!ProgramFiles32.IsEmpty() &&
        SameText(Path.SubString(1, ProgramFiles32.Length()), ProgramFiles32) &&
        !ProgramFiles64.IsEmpty())
    {
      UnicodeString Path64 =
        ProgramFiles64 + Path.SubString(ProgramFiles32.Length() + 1, Path.Length() - ProgramFiles32.Length());
      if (FileExists(ApiPath(Path64)))
      {
        Path = Path64;
        Result = true;
      }
    }
  }

  if (!Result)
  {
    UnicodeString Paths = GetEnvironmentVariable(L"PATH");
    if (!Paths.IsEmpty())
    {
      UnicodeString NewPath = FileSearch(ExtractFileName(Path), Paths);
      Result = !NewPath.IsEmpty();
      if (Result)
      {
        Path = NewPath;
      }
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall OpenSessionInPutty(const UnicodeString PuttyPath,
  TSessionData * SessionData)
{
  UnicodeString Program, AParams, Dir;
  SplitCommand(PuttyPath, Program, AParams, Dir);
  Program = ExpandEnvironmentVariables(Program);
  if (FindFile(Program))
  {

    AParams = ExpandEnvironmentVariables(AParams);
    UnicodeString Password = GUIConfiguration->PuttyPassword ? SessionData->Password : UnicodeString();
    TCustomCommandData Data(SessionData, SessionData->UserName, Password);
    TRemoteCustomCommand RemoteCustomCommand(Data, SessionData->RemoteDirectory);
    TWinInteractiveCustomCommand InteractiveCustomCommand(
      &RemoteCustomCommand, L"PuTTY", UnicodeString());

    UnicodeString Params =
      RemoteCustomCommand.Complete(InteractiveCustomCommand.Complete(AParams, false), true);
    UnicodeString PuttyParams;

    if (!RemoteCustomCommand.IsSiteCommand(AParams))
    {
      UnicodeString SessionName;
      TRegistryStorage * Storage = NULL;
      TSessionData * ExportData = NULL;
      TRegistryStorage * SourceStorage = NULL;
      try
      {
        Storage = new TRegistryStorage(Configuration->PuttySessionsKey);
        Storage->AccessMode = smReadWrite;
        // make it compatible with putty
        Storage->MungeStringValues = false;
        Storage->ForceAnsi = true;
        if (Storage->OpenRootKey(true))
        {
          if (Storage->KeyExists(SessionData->StorageKey))
          {
            SessionName = SessionData->SessionName;
          }
          else
          {
            SourceStorage = new TRegistryStorage(Configuration->PuttySessionsKey);
            SourceStorage->MungeStringValues = false;
            SourceStorage->ForceAnsi = true;
            if (SourceStorage->OpenSubKey(StoredSessions->DefaultSettings->Name, false) &&
                Storage->OpenSubKey(GUIConfiguration->PuttySession, true))
            {
              Storage->Copy(SourceStorage);
              Storage->CloseSubKey();
            }

            ExportData = new TSessionData(L"");
            ExportData->Assign(SessionData);
            ExportData->Modified = true;
            ExportData->Name = GUIConfiguration->PuttySession;
            ExportData->WinTitle = SessionData->SessionName;
            ExportData->Password = L"";

            if (SessionData->FSProtocol == fsFTP)
            {
              if (GUIConfiguration->TelnetForFtpInPutty)
              {
                ExportData->PuttyProtocol = PuttyTelnetProtocol;
                ExportData->PortNumber = TelnetPortNumber;
                // PuTTY  does not allow -pw for telnet
                Password = L"";
              }
              else
              {
                ExportData->PuttyProtocol = PuttySshProtocol;
                ExportData->PortNumber = SshPortNumber;
              }
            }

            ExportData->Save(Storage, true);
            SessionName = GUIConfiguration->PuttySession;
          }
        }
      }
      __finally
      {
        delete Storage;
        delete ExportData;
        delete SourceStorage;
      }

      UnicodeString LoadSwitch = L"-load";
      int P = Params.LowerCase().Pos(LoadSwitch + L" ");
      if ((P == 0) || ((P > 1) && (Params[P - 1] != L' ')))
      {
        AddToList(PuttyParams, FORMAT(L"%s %s", (LoadSwitch, EscapePuttyCommandParam(SessionName))), L" ");
      }
    }

    if (!Password.IsEmpty() && !RemoteCustomCommand.IsPasswordCommand(AParams))
    {
      AddToList(PuttyParams, FORMAT(L"-pw %s", (EscapePuttyCommandParam(Password))), L" ");
    }

    AddToList(PuttyParams, Params, L" ");

    // PuTTY is started in its binary directory to allow relative paths in private key,
    // when opening PuTTY's own stored session.
    ExecuteShellChecked(Program, PuttyParams, true);
  }
  else
  {
    throw Exception(FMTLOAD(FILE_NOT_FOUND, (Program)));
  }
}
//---------------------------------------------------------------------------
bool __fastcall FindTool(const UnicodeString & Name, UnicodeString & Path)
{
  UnicodeString AppPath = IncludeTrailingBackslash(ExtractFilePath(Application->ExeName));
  Path = AppPath + Name;
  bool Result = true;
  if (!FileExists(ApiPath(Path)))
  {
    Path = AppPath + L"PuTTY\\" + Name;
    if (!FileExists(ApiPath(Path)))
    {
      Path = Name;
      if (!FindFile(Path))
      {
        Result = false;
      }
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall CopyCommandToClipboard(const UnicodeString & Command)
{
  bool Result = UseAlternativeFunction() && IsKeyPressed(VK_CONTROL);
  if (Result)
  {
    TInstantOperationVisualizer Visualizer;
    CopyToClipboard(Command);
  }
  return Result;
}
//---------------------------------------------------------------------------
static bool __fastcall DoExecuteShell(const UnicodeString Path, const UnicodeString Params,
  bool ChangeWorkingDirectory, HANDLE * Handle)
{
  bool Result = CopyCommandToClipboard(FormatCommand(Path, Params));

  if (Result)
  {
    if (Handle != NULL)
    {
      *Handle = NULL;
    }
  }
  else
  {
    UnicodeString Directory = ExtractFilePath(Path);

    TShellExecuteInfoW ExecuteInfo;
    memset(&ExecuteInfo, 0, sizeof(ExecuteInfo));
    ExecuteInfo.cbSize = sizeof(ExecuteInfo);
    ExecuteInfo.fMask =
      SEE_MASK_FLAG_NO_UI |
      FLAGMASK((Handle != NULL), SEE_MASK_NOCLOSEPROCESS);
    ExecuteInfo.hwnd = Application->Handle;
    ExecuteInfo.lpFile = (wchar_t*)Path.data();
    ExecuteInfo.lpParameters = (wchar_t*)Params.data();
    ExecuteInfo.lpDirectory = (ChangeWorkingDirectory ? Directory.c_str() : NULL);
    ExecuteInfo.nShow = SW_SHOW;

    Result = (ShellExecuteEx(&ExecuteInfo) != 0);
    if (Result)
    {
      if (Handle != NULL)
      {
        *Handle = ExecuteInfo.hProcess;
      }
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall ExecuteShellChecked(const UnicodeString Path, const UnicodeString Params, bool ChangeWorkingDirectory)
{
  if (!DoExecuteShell(Path, Params, ChangeWorkingDirectory, NULL))
  {
    throw EOSExtException(FMTLOAD(EXECUTE_APP_ERROR, (Path)));
  }
}
//---------------------------------------------------------------------------
void __fastcall ExecuteShellChecked(const UnicodeString Command)
{
  UnicodeString Program, Params, Dir;
  SplitCommand(Command, Program, Params, Dir);
  ExecuteShellChecked(Program, Params);
}
//---------------------------------------------------------------------------
bool __fastcall ExecuteShell(const UnicodeString Path, const UnicodeString Params,
  HANDLE & Handle)
{
  return DoExecuteShell(Path, Params, false, &Handle);
}
//---------------------------------------------------------------------------
void __fastcall ExecuteShellCheckedAndWait(const UnicodeString Command,
  TProcessMessagesEvent ProcessMessages)
{
  UnicodeString Program, Params, Dir;
  SplitCommand(Command, Program, Params, Dir);
  HANDLE ProcessHandle;
  bool Result = DoExecuteShell(Program, Params, false, &ProcessHandle);
  if (!Result)
  {
    throw EOSExtException(FMTLOAD(EXECUTE_APP_ERROR, (Program)));
  }
  else
  {
    if (ProcessHandle != NULL) // only if command was copied to clipboard only
    {
      if (ProcessMessages != NULL)
      {
        unsigned long WaitResult;
        do
        {
          // Same as in ExecuteProcessAndReadOutput
          WaitResult = WaitForSingleObject(ProcessHandle, 200);
          if (WaitResult == WAIT_FAILED)
          {
            throw Exception(LoadStr(DOCUMENT_WAIT_ERROR));
          }
          ProcessMessages();
        }
        while (WaitResult == WAIT_TIMEOUT);
      }
      else
      {
        WaitForSingleObject(ProcessHandle, INFINITE);
      }
    }
  }
}
//---------------------------------------------------------------------------
bool __fastcall SpecialFolderLocation(int PathID, UnicodeString & Path)
{
  LPITEMIDLIST Pidl;
  wchar_t Buf[256];
  if (SHGetSpecialFolderLocation(NULL, PathID, &Pidl) == NO_ERROR &&
      SHGetPathFromIDList(Pidl, Buf))
  {
    Path = UnicodeString(Buf);
    return true;
  }
  return false;
}
//---------------------------------------------------------------------------
UnicodeString __fastcall UniqTempDir(const UnicodeString BaseDir, const UnicodeString Identity,
  bool Mask)
{
  DebugAssert(!BaseDir.IsEmpty());
  UnicodeString TempDir;
  do
  {
    TempDir = IncludeTrailingBackslash(BaseDir) + Identity;
    if (Mask)
    {
      TempDir += L"?????";
    }
    else
    {
      TempDir += IncludeTrailingBackslash(FormatDateTime(L"nnzzz", Now()));
    }
  }
  while (!Mask && DirectoryExists(ApiPath(TempDir)));

  return TempDir;
}
//---------------------------------------------------------------------------
bool __fastcall DeleteDirectory(const UnicodeString DirName)
{
  TSearchRecChecked sr;
  bool retval = true;
  if (FindFirstUnchecked(DirName + L"\\*", faAnyFile, sr) == 0) // VCL Function
  {
    if (FLAGSET(sr.Attr, faDirectory))
    {
      if (sr.Name != L"." && sr.Name != L"..")
        retval = DeleteDirectory(DirName + L"\\" + sr.Name);
    }
    else
    {
      retval = DeleteFile(ApiPath(DirName + L"\\" + sr.Name));
    }

    if (retval)
    {
      while (FindNextChecked(sr) == 0)
      { // VCL Function
        if (FLAGSET(sr.Attr, faDirectory))
        {
          if (sr.Name != L"." && sr.Name != L"..")
            retval = DeleteDirectory(DirName + L"\\" + sr.Name);
        }
        else
        {
          retval = DeleteFile(ApiPath(DirName + L"\\" + sr.Name));
        }

        if (!retval) break;
      }
    }
  }
  FindClose(sr);
  if (retval) retval = RemoveDir(ApiPath(DirName)); // VCL function
  return retval;
}
//---------------------------------------------------------------------------
void __fastcall AddSessionColorImage(
  TCustomImageList * ImageList, TColor Color, int MaskIndex)
{

  // This overly complex drawing is here to support color button on SiteAdvanced
  // dialog. There we use plain TImageList, instead of TPngImageList,
  // TButton does not work with transparent images
  // (not even TBitmap with Transparent = true)
  std::unique_ptr<TBitmap> MaskBitmap(new TBitmap());
  ImageList->GetBitmap(MaskIndex, MaskBitmap.get());

  std::unique_ptr<TPngImage> MaskImage(new TPngImage());
  MaskImage->Assign(MaskBitmap.get());

  std::unique_ptr<TPngImage> ColorImage(new TPngImage(COLOR_RGB, 16, ImageList->Width, ImageList->Height));

  TColor MaskTransparentColor = MaskImage->Pixels[0][0];
  TColor TransparentColor = MaskTransparentColor;
  // Expecting that the color to be replaced is in the centre of the image (HACK)
  TColor MaskColor = MaskImage->Pixels[ImageList->Width / 2][ImageList->Height / 2];

  for (int Y = 0; Y < ImageList->Height; Y++)
  {
    for (int X = 0; X < ImageList->Width; X++)
    {
      TColor SourceColor = MaskImage->Pixels[X][Y];
      TColor DestColor;
      // this branch is pointless as long as MaskTransparentColor and
      // TransparentColor are the same
      if (SourceColor == MaskTransparentColor)
      {
        DestColor = TransparentColor;
      }
      else if (SourceColor == MaskColor)
      {
        DestColor = Color;
      }
      else
      {
        DestColor = SourceColor;
      }
      ColorImage->Pixels[X][Y] = DestColor;
    }
  }

  std::unique_ptr<TBitmap> Bitmap(new TBitmap());
  Bitmap->SetSize(ImageList->Width, ImageList->Height);
  ColorImage->AssignTo(Bitmap.get());

  ImageList->AddMasked(Bitmap.get(), TransparentColor);
}
//---------------------------------------------------------------------------
void __fastcall SetSubmenu(TTBXCustomItem * Item)
{
  class TTBXPublicItem : public TTBXCustomItem
  {
  public:
    __property ItemStyle;
  };
  TTBXPublicItem * PublicItem = reinterpret_cast<TTBXPublicItem *>(Item);
  DebugAssert(PublicItem != NULL);
  // See TTBItemViewer.IsPtInButtonPart (called from TTBItemViewer.MouseDown)
  PublicItem->ItemStyle = PublicItem->ItemStyle << tbisSubmenu;
}
//---------------------------------------------------------------------------
bool __fastcall IsEligibleForApplyingTabs(
  UnicodeString Line, int & TabPos, UnicodeString & Start, UnicodeString & Remaining)
{
  bool Result = false;
  TabPos = Line.Pos(L"\t");
  if (TabPos > 0)
  {
    Remaining = Line.SubString(TabPos + 1, Line.Length() - TabPos);
    // WORKAROUND
    // Some translations still use obsolete hack of consecutive tabs to aling the contents.
    // Delete these, so that the following check does not fail on this
    while (Remaining.SubString(1, 1) == L"\t")
    {
      Remaining.Delete(1, 1);
    }

    // We do not have, not support, mutiple tabs on a single line
    if (DebugAlwaysTrue(Remaining.Pos(L"\t") == 0))
    {
      Start = Line.SubString(1, TabPos - 1);
      // WORKAROUND
      // Previously we padded the string before tab with spaces,
      // to aling the contents across multiple lines
      Start = Start.TrimRight();
      // at least two normal spaces for separation
      Start += L"  ";
      Result = true;
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
static int __fastcall CalculateWidthByLength(UnicodeString Text, void * /*Arg*/)
{
  return Text.Length();
}
//---------------------------------------------------------------------------
void __fastcall ApplyTabs(
  UnicodeString & Text, wchar_t Padding,
  TCalculateWidth CalculateWidth, void * CalculateWidthArg)
{
  if (CalculateWidth == NULL)
  {
    DebugAssert(CalculateWidthArg == NULL);
    CalculateWidth = CalculateWidthByLength;
  }

  std::unique_ptr<TStringList> Lines(TextToStringList(Text));

  int MaxWidth = -1;
  for (int Index = 0; Index < Lines->Count; Index++)
  {
    UnicodeString Line = Lines->Strings[Index];
    int TabPos;
    UnicodeString Start;
    UnicodeString Remaining;
    if (IsEligibleForApplyingTabs(Line, TabPos, Start, Remaining))
    {
      int Width = CalculateWidth(Start, CalculateWidthArg);
      MaxWidth = Max(MaxWidth, Width);
    }
  }

  // Optimization and also to prevent potential regression for texts without tabs
  if (MaxWidth >= 0)
  {
    for (int Index = 0; Index < Lines->Count; Index++)
    {
      UnicodeString Line = Lines->Strings[Index];
      int TabPos;
      UnicodeString Start;
      UnicodeString Remaining;
      if (IsEligibleForApplyingTabs(Line, TabPos, Start, Remaining))
      {
        int Width;
        int Iterations = 0;
        while ((Width = CalculateWidth(Start, CalculateWidthArg)) < MaxWidth)
        {
          int Wider = CalculateWidth(Start + Padding, CalculateWidthArg);
          // If padded string is wider than max width by more pixels
          // than non-padded string is shorter than max width
          if ((Wider > MaxWidth) && ((Wider - MaxWidth) > (MaxWidth - Width)))
          {
            break;
          }
          Start += Padding;
          Iterations++;
          // In rare case a tab is zero-width with some strange font (like HYLE)
          if (Iterations > 100)
          {
            break;
          }
        }
        Lines->Strings[Index] = Start + Remaining;
      }
    }

    Text = Lines->Text;
    // remove trailing newline
    Text = Text.TrimRight();
  }
}
//---------------------------------------------------------------------------
static void __fastcall DoSelectScaledImageList(TImageList * ImageList)
{
  TImageList * MatchingList = NULL;
  int MachingPixelsPerInch = 0;
  int PixelsPerInch = GetComponentPixelsPerInch(ImageList);

  for (int Index = 0; Index < ImageList->Owner->ComponentCount; Index++)
  {
    TImageList * OtherList = dynamic_cast<TImageList *>(ImageList->Owner->Components[Index]);

    if ((OtherList != NULL) &&
        (OtherList != ImageList) &&
        StartsStr(ImageList->Name, OtherList->Name))
    {
      UnicodeString OtherListPixelsPerInchStr =
        OtherList->Name.SubString(
          ImageList->Name.Length() + 1, OtherList->Name.Length() - ImageList->Name.Length());
      int OtherListPixelsPerInch = StrToInt(OtherListPixelsPerInchStr);
      if ((OtherListPixelsPerInch <= PixelsPerInch) &&
          ((MatchingList == NULL) ||
           (MachingPixelsPerInch < OtherListPixelsPerInch)))
      {
        MatchingList = OtherList;
        MachingPixelsPerInch = OtherListPixelsPerInch;
      }
    }
  }

  if (MatchingList != NULL)
  {
    UnicodeString ImageListBackupName = ImageList->Name + IntToStr(USER_DEFAULT_SCREEN_DPI);

    if (ImageList->Owner->FindComponent(ImageListBackupName) == NULL)
    {
      TImageList * ImageListBackup;
      TPngImageList * PngImageList = dynamic_cast<TPngImageList *>(ImageList);
      if (PngImageList != NULL)
      {
        ImageListBackup = new TPngImageList(ImageList->Owner);
      }
      else
      {
        ImageListBackup = new TImageList(ImageList->Owner);
      }

      ImageListBackup->Name = ImageListBackupName;
      ImageList->Owner->InsertComponent(ImageListBackup);
      CopyImageList(ImageListBackup, ImageList);
    }

    CopyImageList(ImageList, MatchingList);
  }
}
//---------------------------------------------------------------------------
static void __fastcall ImageListRescale(TComponent * Sender, TObject * /*Token*/)
{
  TImageList * ImageList = DebugNotNull(dynamic_cast<TImageList *>(Sender));
  DoSelectScaledImageList(ImageList);
}
//---------------------------------------------------------------------------
void __fastcall SelectScaledImageList(TImageList * ImageList)
{
  DoSelectScaledImageList(ImageList);

  SetRescaleFunction(ImageList, ImageListRescale);
}
//---------------------------------------------------------------------------
void __fastcall CopyImageList(TImageList * TargetList, TImageList * SourceList)
{
  // Maybe this is not necessary, once the TPngImageList::Assign was fixed
  TPngImageList * PngTargetList = dynamic_cast<TPngImageList *>(TargetList);
  TPngImageList * PngSourceList = dynamic_cast<TPngImageList *>(SourceList);

  TargetList->Clear();
  TargetList->Height = SourceList->Height;
  TargetList->Width = SourceList->Width;

  if ((PngTargetList != NULL) && (PngSourceList != NULL))
  {
    // AddImages won't copy the names and we need them for
    // LoadDialogImage and TFrameAnimation
    PngTargetList->PngImages->Assign(PngSourceList->PngImages);
  }
  else
  {
    TargetList->AddImages(SourceList);
  }
}
//---------------------------------------------------------------------------
static bool __fastcall DoLoadDialogImage(TImage * Image, const UnicodeString & ImageName)
{
  bool Result = false;
  if (GlyphsModule != NULL)
  {
    TPngImageList * DialogImages = GetDialogImages(Image);

    int Index;
    for (Index = 0; Index < DialogImages->PngImages->Count; Index++)
    {
      TPngImageCollectionItem * PngItem = DialogImages->PngImages->Items[Index];
      if (SameText(PngItem->Name, ImageName))
      {
        Image->Picture->Assign(PngItem->PngImage);
        break;
      }
    }

    DebugAssert(Index < DialogImages->PngImages->Count);
    Result = true;
  }
  // When showing an exception from wWinMain, the images are released already.
  // Non-existence of the glyphs module is just a good indication of that.
  // We expect errors only.
  else if (ImageName == L"Error")
  {
    Image->Picture->Icon->Handle = LoadIcon(0, IDI_HAND);
  }
  // For showing an information about trace files
  else if (DebugAlwaysTrue(ImageName == L"Information"))
  {
    Image->Picture->Icon->Handle = LoadIcon(0, IDI_APPLICATION);
  }
  return Result;
}
//---------------------------------------------------------------------------
class TDialogImageName : public TObject
{
public:
  UnicodeString ImageName;
};
//---------------------------------------------------------------------------
static void __fastcall DialogImageRescale(TComponent * Sender, TObject * Token)
{
  TImage * Image = DebugNotNull(dynamic_cast<TImage *>(Sender));
  TDialogImageName * DialogImageName = DebugNotNull(dynamic_cast<TDialogImageName *>(Token));
  DoLoadDialogImage(Image, DialogImageName->ImageName);
}
//---------------------------------------------------------------------------
void __fastcall LoadDialogImage(TImage * Image, const UnicodeString & ImageName)
{
  if (DoLoadDialogImage(Image, ImageName))
  {
    TDialogImageName * DialogImageName = new TDialogImageName();
    DialogImageName->ImageName = ImageName;
    SetRescaleFunction(Image, DialogImageRescale, DialogImageName, true);
  }
}
//---------------------------------------------------------------------------
int __fastcall DialogImageSize(TForm * Form)
{
  return ScaleByPixelsPerInch(32, Form);
}
//---------------------------------------------------------------------------
void __fastcall HideComponentsPanel(TForm * Form)
{
  TComponent * Component = DebugNotNull(Form->FindComponent(L"ComponentsPanel"));
  TPanel * Panel = DebugNotNull(dynamic_cast<TPanel *>(Component));
  DebugAssert(Panel->Align == alBottom);
  int Offset = Panel->Height;
  Panel->Visible = false;
  Panel->Height = 0;
  Form->Height -= Offset;

  for (int Index = 0; Index < Form->ControlCount; Index++)
  {
    TControl * Control = Form->Controls[Index];

    // Shift back bottom-anchored controls
    // (needed for toolbar panel on Progress window and buttons on Preferences dialog),
    if ((Control->Align == alNone) &&
        Control->Anchors.Contains(akBottom) &&
        !Control->Anchors.Contains(akTop))
    {
      Control->Top += Offset;
    }

    // Resize back all-anchored controls
    // (needed for main panel on Preferences dialog),
    if (Control->Anchors.Contains(akBottom) &&
        Control->Anchors.Contains(akTop))
    {
      Control->Height += Offset;
    }
  }
}
//---------------------------------------------------------------------------
class TBrowserViewer : public TWebBrowserEx
{
public:
  __fastcall virtual TBrowserViewer(TComponent* AOwner);

  void __fastcall AddLinkHandler(
    const UnicodeString & Url, TNotifyEvent Handler);
  void __fastcall NavigateToUrl(const UnicodeString & Url);

  TControl * LoadingPanel;

protected:
  DYNAMIC void __fastcall DoContextPopup(const TPoint & MousePos, bool & Handled);
  void __fastcall DocumentComplete(
    TObject * Sender, const _di_IDispatch Disp, const OleVariant & URL);
  void __fastcall BeforeNavigate2(
    TObject * Sender, const _di_IDispatch Disp, const OleVariant & URL,
    const OleVariant & Flags, const OleVariant & TargetFrameName,
    const OleVariant & PostData, const OleVariant & Headers, WordBool & Cancel);

  bool FComplete;
  std::map<UnicodeString, TNotifyEvent> FHandlers;
};
//---------------------------------------------------------------------------
__fastcall TBrowserViewer::TBrowserViewer(TComponent* AOwner) :
  TWebBrowserEx(AOwner)
{
  FComplete = false;

  OnDocumentComplete = DocumentComplete;
  OnBeforeNavigate2 = BeforeNavigate2;
  LoadingPanel = NULL;
}
//---------------------------------------------------------------------------
void __fastcall TBrowserViewer::AddLinkHandler(
  const UnicodeString & Url, TNotifyEvent Handler)
{
  FHandlers.insert(std::make_pair(Url, Handler));
}
//---------------------------------------------------------------------------
void __fastcall TBrowserViewer::DoContextPopup(const TPoint & MousePos, bool & Handled)
{
  // suppress built-in context menu
  Handled = true;
  TWebBrowserEx::DoContextPopup(MousePos, Handled);
}
//---------------------------------------------------------------------------
void __fastcall TBrowserViewer::DocumentComplete(
    TObject * /*Sender*/, const _di_IDispatch /*Disp*/, const OleVariant & /*URL*/)
{
  SetBrowserDesignModeOff(this);

  FComplete = true;

  if (LoadingPanel != NULL)
  {
    LoadingPanel->Visible = false;
  }
}
//---------------------------------------------------------------------------
void __fastcall TBrowserViewer::BeforeNavigate2(
  TObject * /*Sender*/, const _di_IDispatch /*Disp*/, const OleVariant & AURL,
  const OleVariant & /*Flags*/, const OleVariant & /*TargetFrameName*/,
  const OleVariant & /*PostData*/, const OleVariant & /*Headers*/, WordBool & Cancel)
{
  // If OnDocumentComplete was not called yet, is has to be our initial message URL,
  // opened using TWebBrowserEx::Navigate(), allow it.
  // Otherwise it's user navigating, block that and open link
  // in an external browser, possibly adding campaign parameters on the way.
  if (FComplete)
  {
    Cancel = 1;

    UnicodeString URL = AURL;
    if (FHandlers.count(URL) > 0)
    {
      FHandlers[URL](this);
    }
    else
    {
      OpenBrowser(URL);
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TBrowserViewer::NavigateToUrl(const UnicodeString & Url)
{
  FComplete = false;
  Navigate(Url.c_str());
}
//---------------------------------------------------------------------------
TPanel * __fastcall CreateLabelPanel(TPanel * Parent, const UnicodeString & Label)
{
  TPanel * Result = CreateBlankPanel(Parent);
  Result->Parent = Parent;
  Result->Align = alClient;
  Result->Caption = Label;
  return Result;
}
//---------------------------------------------------------------------------
TWebBrowserEx * __fastcall CreateBrowserViewer(TPanel * Parent, const UnicodeString & LoadingLabel)
{
  TBrowserViewer * Result = new TBrowserViewer(Parent);
  // TWebBrowserEx has its own unrelated Name and Parent properties.
  // The name is used in DownloadUpdate().
  static_cast<TWinControl *>(Result)->Name = L"BrowserViewer";
  static_cast<TWinControl *>(Result)->Parent = Parent;
  Result->Align = alClient;
  Result->ControlBorder = cbNone;

  Result->LoadingPanel = CreateLabelPanel(Parent, LoadingLabel);

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall SetBrowserDesignModeOff(TWebBrowserEx * WebBrowser)
{
  if (DebugAlwaysTrue(WebBrowser->Document2 != NULL))
  {
    WebBrowser->Document2->designMode = L"Off";
  }
}
//---------------------------------------------------------------------------
void __fastcall AddBrowserLinkHandler(TWebBrowserEx * WebBrowser,
  const UnicodeString & Url, TNotifyEvent Handler)
{
  TBrowserViewer * BrowserViewer = dynamic_cast<TBrowserViewer *>(WebBrowser);
  if (DebugAlwaysTrue(BrowserViewer != NULL))
  {
    BrowserViewer->AddLinkHandler(Url, Handler);
  }
}
//---------------------------------------------------------------------------
void __fastcall NavigateBrowserToUrl(TWebBrowserEx * WebBrowser, const UnicodeString & Url)
{
  TBrowserViewer * BrowserViewer = dynamic_cast<TBrowserViewer *>(WebBrowser);
  if (DebugAlwaysTrue(BrowserViewer != NULL))
  {
    BrowserViewer->NavigateToUrl(Url);
  }
}
//---------------------------------------------------------------------------
TComponent * __fastcall FindComponentRecursively(TComponent * Root, const UnicodeString & Name)
{
  for (int Index = 0; Index < Root->ComponentCount; Index++)
  {
    TComponent * Component = Root->Components[Index];
    if (CompareText(Component->Name, Name) == 0)
    {
      return Component;
    }

    Component = FindComponentRecursively(Component, Name);
    if (Component != NULL)
    {
      return Component;
    }
  }
  return NULL;
}
//---------------------------------------------------------------------------
TLocalCustomCommand::TLocalCustomCommand()
{
}
//---------------------------------------------------------------------------
TLocalCustomCommand::TLocalCustomCommand(
    const TCustomCommandData & Data, const UnicodeString & RemotePath, const UnicodeString & LocalPath) :
  TFileCustomCommand(Data, RemotePath)
{
  FLocalPath = LocalPath;
}
//---------------------------------------------------------------------------
TLocalCustomCommand::TLocalCustomCommand(const TCustomCommandData & Data,
  const UnicodeString & RemotePath, const UnicodeString & LocalPath, const UnicodeString & FileName,
  const UnicodeString & LocalFileName, const UnicodeString & FileList) :
  TFileCustomCommand(Data, RemotePath, FileName, FileList)
{
  FLocalPath = LocalPath;
  FLocalFileName = LocalFileName;
}
//---------------------------------------------------------------------------
int __fastcall TLocalCustomCommand::PatternLen(const UnicodeString & Command, int Index)
{
  int Len;
  if ((Index < Command.Length()) && (Command[Index + 1] == L'\\'))
  {
    Len = 2;
  }
  else if ((Index < Command.Length()) && (Command[Index + 1] == L'^'))
  {
    Len = 3;
  }
  else
  {
    Len = TFileCustomCommand::PatternLen(Command, Index);
  }
  return Len;
}
//---------------------------------------------------------------------------
bool __fastcall TLocalCustomCommand::PatternReplacement(
  int Index, const UnicodeString & Pattern, UnicodeString & Replacement, bool & Delimit)
{
  bool Result;
  if (Pattern == L"!\\")
  {
    // When used as "!\" in an argument to PowerShell, the trailing \ would escpae the ",
    // so we exclude it
    Replacement = ExcludeTrailingBackslash(FLocalPath);
    Result = true;
  }
  else if (Pattern == L"!^!")
  {
    Replacement = FLocalFileName;
    Result = true;
  }
  else
  {
    Result = TFileCustomCommand::PatternReplacement(Index, Pattern, Replacement, Delimit);
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TLocalCustomCommand::DelimitReplacement(
  UnicodeString & /*Replacement*/, wchar_t /*Quote*/)
{
  // never delimit local commands
}
//---------------------------------------------------------------------------
bool __fastcall TLocalCustomCommand::HasLocalFileName(const UnicodeString & Command)
{
  return FindPattern(Command, L'^');
}
//---------------------------------------------------------------------------
bool __fastcall TLocalCustomCommand::IsFileCommand(const UnicodeString & Command)
{
  return TFileCustomCommand::IsFileCommand(Command) || HasLocalFileName(Command);
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
typedef std::set<TDataModule *> TImagesModules;
static TImagesModules ImagesModules;
static std::map<int, TPngImageList *> AnimationsImages;
static std::map<int, TImageList *> ButtonImages;
static std::map<int, TPngImageList *> DialogImages;
//---------------------------------------------------------------------------
int __fastcall NormalizePixelsPerInch(int PixelsPerInch)
{
  if (PixelsPerInch >= 192)
  {
    PixelsPerInch = 192;
  }
  else if (PixelsPerInch >= 144)
  {
    PixelsPerInch = 144;
  }
  else if (PixelsPerInch >= 120)
  {
    PixelsPerInch = 120;
  }
  else
  {
    PixelsPerInch = 96;
  }
  return PixelsPerInch;
}
//---------------------------------------------------------------------------
static int __fastcall NeedImagesModule(TControl * Control)
{
  int PixelsPerInch = NormalizePixelsPerInch(GetControlPixelsPerInch(Control));

  if (AnimationsImages.find(PixelsPerInch) == AnimationsImages.end())
  {
    TDataModule * ImagesModule;
    HANDLE ResourceModule = GUIConfiguration->ChangeToDefaultResourceModule();
    try
    {
      if (PixelsPerInch == 192)
      {
        ImagesModule = new TAnimations192Module(Application);
      }
      else if (PixelsPerInch == 144)
      {
        ImagesModule = new TAnimations144Module(Application);
      }
      else if (PixelsPerInch == 120)
      {
        ImagesModule = new TAnimations120Module(Application);
      }
      else
      {
        DebugAssert(PixelsPerInch == 96);
        ImagesModule = new TAnimations96Module(Application);
      }

      ImagesModules.insert(ImagesModule);

      TPngImageList * AAnimationImages =
        DebugNotNull(dynamic_cast<TPngImageList *>(ImagesModule->FindComponent(L"AnimationImages")));
      AnimationsImages.insert(std::make_pair(PixelsPerInch, AAnimationImages));

      TImageList * AButtonImages =
        DebugNotNull(dynamic_cast<TImageList *>(ImagesModule->FindComponent(L"ButtonImages")));
      ButtonImages.insert(std::make_pair(PixelsPerInch, AButtonImages));

      TPngImageList * ADialogImages =
        DebugNotNull(dynamic_cast<TPngImageList *>(ImagesModule->FindComponent(L"DialogImages")));
      DialogImages.insert(std::make_pair(PixelsPerInch, ADialogImages));
    }
    __finally
    {
      GUIConfiguration->ChangeResourceModule(ResourceModule);
    }
  }

  return PixelsPerInch;
}
//---------------------------------------------------------------------------
TPngImageList * __fastcall GetAnimationsImages(TControl * Control)
{
  int PixelsPerInch = NeedImagesModule(Control);
  return DebugNotNull(AnimationsImages[PixelsPerInch]);
}
//---------------------------------------------------------------------------
TImageList * __fastcall GetButtonImages(TControl * Control)
{
  int PixelsPerInch = NeedImagesModule(Control);
  return DebugNotNull(ButtonImages[PixelsPerInch]);
}
//---------------------------------------------------------------------------
TPngImageList * __fastcall GetDialogImages(TControl * Control)
{
  int PixelsPerInch = NeedImagesModule(Control);
  return DebugNotNull(DialogImages[PixelsPerInch]);
}
//---------------------------------------------------------------------------
void __fastcall ReleaseImagesModules()
{

  TImagesModules::iterator i = ImagesModules.begin();
  while (i != ImagesModules.end())
  {
    delete (*i);
    i++;
  }
  ImagesModules.clear();
}
//---------------------------------------------------------------------------
__fastcall TFrameAnimation::TFrameAnimation()
{
  FFirstFrame = -1;
}
//---------------------------------------------------------------------------
void __fastcall TFrameAnimation::Init(TPaintBox * PaintBox, const UnicodeString & Name)
{
  FName = Name;
  FPaintBox = PaintBox;

  FPaintBox->ControlStyle = FPaintBox->ControlStyle << csOpaque;
  DebugAssert((FPaintBox->OnPaint == NULL) || (FPaintBox->OnPaint == PaintBoxPaint));
  FPaintBox->OnPaint = PaintBoxPaint;
  SetRescaleFunction(FPaintBox, PaintBoxRescale, reinterpret_cast<TObject *>(this));

  DoInit();
}
//---------------------------------------------------------------------------
void __fastcall TFrameAnimation::DoInit()
{
  FImageList = GetAnimationsImages(FPaintBox);
  FFirstFrame = -1;
  FFirstLoopFrame = -1;
  FPaintBox->Width = FImageList->Width;
  FPaintBox->Height = FImageList->Height;

  if (!FName.IsEmpty())
  {
    int Frame = 0;
    while (Frame < FImageList->PngImages->Count)
    {
      UnicodeString FrameData = FImageList->PngImages->Items[Frame]->Name;
      UnicodeString FrameName;
      FrameName = CutToChar(FrameData, L'_', false);

      if (SameText(FName, FrameName))
      {
        int FrameIndex = StrToInt(CutToChar(FrameData, L'_', false));
        if (FFirstFrame < 0)
        {
          FFirstFrame = Frame;
        }
        if ((FFirstLoopFrame < 0) && (FrameIndex > 0))
        {
          FFirstLoopFrame = Frame;
        }
        FLastFrame = Frame;
      }
      else
      {
        if (FFirstFrame >= 0)
        {
          // optimization
          break;
        }
      }
      Frame++;
    }

    DebugAssert(FFirstFrame >= 0);
    DebugAssert(FFirstLoopFrame >= 0);
  }

  Stop();
}
//---------------------------------------------------------------------------
void __fastcall TFrameAnimation::PaintBoxRescale(TComponent * /*Sender*/, TObject * Token)
{
  TFrameAnimation * FrameAnimation = reinterpret_cast<TFrameAnimation *>(Token);
  FrameAnimation->Rescale();
}
//---------------------------------------------------------------------------
void __fastcall TFrameAnimation::Rescale()
{
  bool Started = (FTimer != NULL) && FTimer->Enabled;
  DoInit();
  if (Started)
  {
    Start();
  }
}
//---------------------------------------------------------------------------
void __fastcall TFrameAnimation::Start()
{
  if (FFirstFrame >= 0)
  {
    FNextFrameTick = GetTickCount();
    CalculateNextFrameTick();

    if (FTimer == NULL)
    {
      FTimer = new TTimer(GetParentForm(FPaintBox));
      FTimer->Interval = static_cast<int>(GUIUpdateInterval);
      FTimer->OnTimer = Timer;
    }
    else
    {
      // reset timer
      FTimer->Enabled = false;
      FTimer->Enabled = true;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TFrameAnimation::Timer(TObject * /*Sender*/)
{
  Animate();
}
//---------------------------------------------------------------------------
void __fastcall TFrameAnimation::PaintBoxPaint(TObject * Sender)
{
  if (FFirstFrame >= 0)
  {
    // Double-buffered drawing to prevent flicker (as the images are transparent)
    DebugUsedParam(Sender);
    DebugAssert(FPaintBox == Sender);
    DebugAssert(FPaintBox->ControlStyle.Contains(csOpaque));
    std::unique_ptr<TBitmap> Bitmap(new TBitmap());
    Bitmap->SetSize(FPaintBox->Width, FPaintBox->Height);
    Bitmap->Canvas->Brush->Color = FPaintBox->Color;
    TRect Rect(0, 0, Bitmap->Width, Bitmap->Height);
    Bitmap->Canvas->FillRect(Rect);
    TGraphic * Graphic = GetCurrentImage()->PngImage;
    DebugAssert(Graphic->Width == FPaintBox->Width);
    DebugAssert(Graphic->Height == FPaintBox->Height);
    Bitmap->Canvas->Draw(0, 0, Graphic);
    FPaintBox->Canvas->Draw(0, 0, Bitmap.get());
  }
  FPainted = true;
}
//---------------------------------------------------------------------------
void __fastcall TFrameAnimation::Repaint()
{
  FPainted = false;
  // If the form is not showing yet, the Paint() is not even called
  FPaintBox->Repaint();
  if (!FPainted)
  {
    // Paint later, alternativelly we may keep trying Repaint() in Animate().
    // See also a hack in TAuthenticateForm::Log.
    FPaintBox->Invalidate();
  }
}
//---------------------------------------------------------------------------
void __fastcall TFrameAnimation::Stop()
{
  FNextFrameTick = std::numeric_limits<DWORD>::max();
  FCurrentFrame = FFirstFrame;
  Repaint();

  if (FTimer != NULL)
  {
    FTimer->Enabled = false;
  }
}
//---------------------------------------------------------------------------
void __fastcall TFrameAnimation::Animate()
{
  if (FFirstFrame >= 0)
  {
    // UPGRADE: Use GetTickCount64() when we stop supporting Windows XP.
    DWORD TickCount = GetTickCount();

    // Keep in sync with an opposite condition at the end of the loop.
    // We may skip some frames if we got stalled for a while
    while (TickCount >= FNextFrameTick)
    {
      if (FCurrentFrame >= FLastFrame)
      {
        FCurrentFrame = FFirstLoopFrame;
      }
      else
      {
        FCurrentFrame++;
      }

      CalculateNextFrameTick();

      Repaint();
    }
  }
}
//---------------------------------------------------------------------------
TPngImageCollectionItem * __fastcall TFrameAnimation::GetCurrentImage()
{
  return FImageList->PngImages->Items[FCurrentFrame];
}
//---------------------------------------------------------------------------
void __fastcall TFrameAnimation::CalculateNextFrameTick()
{
  TPngImageCollectionItem * ImageItem = GetCurrentImage();
  UnicodeString Duration = ImageItem->Name;
  CutToChar(Duration, L'_', false);
  // skip index (is not really used)
  CutToChar(Duration, L'_', false);
  // This should overflow, when tick count wraps.
  FNextFrameTick += StrToInt(Duration) * 10;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Hints use:
// - Cleanup list tooltip (multi line)
// - Combo edit button
// - Transfer settings label (multi line, follows label size and font)
// - HintLabel (hint and persistent hint, multipline)
// - status bar hints
//---------------------------------------------------------------------------
__fastcall TScreenTipHintWindow::TScreenTipHintWindow(TComponent * Owner) :
  THintWindow(Owner)
{
  FParentPainting = false;
}
//---------------------------------------------------------------------------
int __fastcall TScreenTipHintWindow::GetTextFlags(TControl * Control)
{
  return DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | Control->DrawTextBiDiModeFlagsReadingOnly();
}
//---------------------------------------------------------------------------
bool __fastcall TScreenTipHintWindow::UseBoldShortHint(TControl * HintControl)
{
  return
    (dynamic_cast<TTBCustomDockableWindow *>(HintControl) != NULL) ||
    (dynamic_cast<TTBPopupWindow *>(HintControl) != NULL);
}
//---------------------------------------------------------------------------
bool __fastcall TScreenTipHintWindow::IsPathLabel(TControl * HintControl)
{
  return (dynamic_cast<TPathLabel *>(HintControl) != NULL);
}
//---------------------------------------------------------------------------
bool __fastcall TScreenTipHintWindow::IsHintPopup(TControl * HintControl, const UnicodeString & Hint)
{
  TLabel * HintLabel = dynamic_cast<TLabel *>(HintControl);
  return (HintLabel != NULL) && HasLabelHintPopup(HintLabel, Hint);
}
//---------------------------------------------------------------------------
int __fastcall TScreenTipHintWindow::GetMargin(TControl * HintControl, const UnicodeString & Hint)
{
  int Result;

  if (IsHintPopup(HintControl, Hint) || IsPathLabel(HintControl))
  {
    Result = 3;
  }
  else
  {
    Result = 6;
  }

  Result = ScaleByTextHeight(HintControl, Result);

  return Result;
}
//---------------------------------------------------------------------------
TFont * __fastcall TScreenTipHintWindow::GetFont(TControl * HintControl, const UnicodeString & Hint)
{
  TFont * Result;
  if (IsHintPopup(HintControl, Hint) || IsPathLabel(HintControl))
  {
    Result = reinterpret_cast<TLabel *>(dynamic_cast<TCustomLabel *>(HintControl))->Font;
  }
  else
  {
    FScaledHintFont.reset(new TFont());
    FScaledHintFont->Assign(Screen->HintFont);
    FScaledHintFont->Size = ScaleByPixelsPerInchFromSystem(FScaledHintFont->Size, HintControl);
    Result = FScaledHintFont.get();
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TScreenTipHintWindow::CalcHintTextRect(TControl * Control, TCanvas * Canvas, TRect & Rect, const UnicodeString & Hint)
{
  const int Flags = DT_CALCRECT | GetTextFlags(Control);
  DrawText(Canvas->Handle, Hint.c_str(), -1, &Rect, Flags);
}
//---------------------------------------------------------------------------
TRect __fastcall TScreenTipHintWindow::CalcHintRect(int MaxWidth, const UnicodeString AHint, void * AData)
{
  TControl * HintControl = GetHintControl(AData);
  int Margin = GetMargin(HintControl, AHint);
  const UnicodeString ShortHint = GetShortHint(AHint);
  const UnicodeString LongHint = GetLongHintIfAny(AHint);

  Canvas->Font->Assign(GetFont(HintControl, AHint));

  const int ScreenTipTextOnlyWidth = ScaleByTextHeight(HintControl, cScreenTipTextOnlyWidth);

  if (!LongHint.IsEmpty())
  {
    // double-margin on the right
    MaxWidth = ScreenTipTextOnlyWidth - (3 * Margin);
  }

  // Multi line short hints can be twice as wide, to not break the individual lines unless really necessary.
  // (login site tree, clean up dialog list, preferences custom command list, persistent hint, etc).
  // And they also can be twice as wide, to not break the individual lines unless really necessary.
  if (ShortHint.Pos(L"\n") > 0)
  {
    MaxWidth *= 2;
  }

  bool HintPopup = IsHintPopup(HintControl, AHint);
  if (HintPopup)
  {
    MaxWidth = HintControl->Width;
  }

  if (UseBoldShortHint(HintControl))
  {
    Canvas->Font->Style = TFontStyles() << fsBold;
  }
  TRect ShortRect(0, 0, MaxWidth, 0);
  CalcHintTextRect(this, Canvas, ShortRect, ShortHint);
  Canvas->Font->Style = TFontStyles();

  TRect Result;

  if (LongHint.IsEmpty())
  {
    Result = ShortRect;

    if (HintPopup)
    {
      Result.Right = MaxWidth + 2 * Margin;
    }
    else
    {
      Result.Right += 3 * Margin;
    }

    Result.Bottom += 2 * Margin;
  }
  else
  {
    const int LongIndentation = Margin * 3 / 2;
    TRect LongRect(0, 0, MaxWidth - LongIndentation, 0);
    CalcHintTextRect(this, Canvas, LongRect, LongHint);

    Result.Right = ScreenTipTextOnlyWidth;
    Result.Bottom = Margin + ShortRect.Height() + (Margin / 3 * 5) + LongRect.Height() + Margin;
  }

  // VCLCOPY: To counter the increase in THintWindow::ActivateHintData
  Result.Bottom -= 4;

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TScreenTipHintWindow::ActivateHintData(const TRect & ARect, const UnicodeString AHint, void * AData)
{
  FShortHint = GetShortHint(AHint);
  FLongHint = GetLongHintIfAny(AHint);
  FHintControl = GetHintControl(AData);
  FMargin = GetMargin(FHintControl, AHint);
  FHintPopup = IsHintPopup(FHintControl, AHint);

  Canvas->Font->Assign(GetFont(FHintControl, AHint));

  TRect Rect = ARect;

  if (FHintPopup)
  {
    Rect.SetLocation(FHintControl->ClientToScreen(TPoint(-FMargin, -FMargin)));
  }
  if (IsPathLabel(FHintControl))
  {
    Rect.Offset(-FMargin, -FMargin);
  }

  THintWindow::ActivateHintData(Rect, FShortHint, AData);
}
//---------------------------------------------------------------------------
TControl * __fastcall TScreenTipHintWindow::GetHintControl(void * Data)
{
  return reinterpret_cast<TControl *>(DebugNotNull(Data));
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TScreenTipHintWindow::GetLongHintIfAny(const UnicodeString & AHint)
{
  UnicodeString Result;
  int P = Pos(L"|", AHint);
  if (P > 0)
  {
    Result = GetLongHint(AHint);
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TScreenTipHintWindow::Dispatch(void * AMessage)
{
  TMessage * Message = static_cast<TMessage*>(AMessage);
  switch (Message->Msg)
  {
    case WM_GETTEXTLENGTH:
      if (FParentPainting)
      {
        // make THintWindow::Paint() not paint the Caption
        Message->Result = 0;
      }
      else
      {
        THintWindow::Dispatch(AMessage);
      }
      break;

    default:
      THintWindow::Dispatch(AMessage);
      break;
  }
}
//---------------------------------------------------------------------------
void __fastcall TScreenTipHintWindow::Paint()
{
  // paint frame/background
  {
    TAutoFlag ParentPaintingFlag(FParentPainting);
    THintWindow::Paint();
  }

  const int Flags = GetTextFlags(this);
  const int Margin = FMargin - 1; // 1 = border

  TRect Rect = ClientRect;
  Rect.Inflate(-Margin, -Margin);
  if (!FHintPopup)
  {
    Rect.Right -= FMargin;
  }
  if (UseBoldShortHint(FHintControl))
  {
    Canvas->Font->Style = TFontStyles() << fsBold;
  }
  DrawText(Canvas->Handle, FShortHint.c_str(), -1, &Rect, Flags);
  TRect ShortRect = Rect;
  DrawText(Canvas->Handle, FShortHint.c_str(), -1, &ShortRect, DT_CALCRECT | Flags);
  Canvas->Font->Style = TFontStyles();

  if (!FLongHint.IsEmpty())
  {
    Rect.Left += FMargin * 3 / 2;
    Rect.Top += ShortRect.Height() + (FMargin / 3 * 5);
    DrawText(Canvas->Handle, FLongHint.c_str(), -1, &Rect, Flags);
  }
}

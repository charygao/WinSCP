//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include <Common.h>
#include <WinInterface.h>
#include <CoreMain.h>
#include <TextsWin.h>
#include <VCLCommon.h>
#include <CustomWinConfiguration.h>
#include <Tools.h>
#include <GUITools.h>

#include "Copy.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "Rights"
#pragma link "CopyParams"
#pragma link "HistoryComboBox"
#ifndef NO_RESOURCES
#pragma resource "*.dfm"
#endif
//---------------------------------------------------------------------------
bool __fastcall DoCopyDialog(bool ToRemote,
  bool Move, TStrings * FileList, UnicodeString & TargetDirectory,
  TGUICopyParamType * Params, int Options, int CopyParamAttrs, TSessionData * SessionData,
  int * OutputOptions)
{
  bool Result;
  TCopyDialog *CopyDialog = new TCopyDialog(Application, ToRemote, Move, FileList, Options, CopyParamAttrs, SessionData);
  try
  {
    if (FLAGSET(CopyParamAttrs, cpaNoTransferMode))
    {
      // If local and remote EOL types are the same, there is no need
      // for ASCII (or Automatic) mode
      Params->TransferMode = tmBinary;
    }
    if (OutputOptions != NULL)
    {
      CopyDialog->OutputOptions = *OutputOptions;
    }
    CopyDialog->Directory = TargetDirectory;
    CopyDialog->Params = *Params;
    Result = CopyDialog->Execute();
    if (Result)
    {
      TargetDirectory = CopyDialog->Directory;
      *Params = CopyDialog->Params;
      if (OutputOptions != NULL)
      {
        *OutputOptions = CopyDialog->OutputOptions;
      }
    }
  }
  __finally
  {
    delete CopyDialog;
  }
  return Result;
}
//---------------------------------------------------------------------------
__fastcall TCopyDialog::TCopyDialog(
  TComponent* Owner, bool ToRemote, bool Move, TStrings * FileList, int Options,
  int CopyParamAttrs, TSessionData * SessionData) : TForm(Owner)
{
  FToRemote = ToRemote;
  FMove = Move;
  FOptions = Options;
  FCopyParamAttrs = CopyParamAttrs;
  FFileList = FileList;
  FSessionData = SessionData;

  FOutputOptions = 0;

  AdjustControls();

  FPresetsMenu = new TPopupMenu(this);

  HotTrackLabel(CopyParamLabel);
  CopyParamListButton(TransferSettingsButton);
  HotTrackLabel(ShortCutHintLabel);

  UseSystemSettings(this);
}
//---------------------------------------------------------------------------
__fastcall TCopyDialog::~TCopyDialog()
{
  delete FPresetsMenu;
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::AdjustTransferControls()
{
  if (FFileList && FFileList->Count)
  {
    if (!FToRemote && !FMove && FLAGSET(FOutputOptions, cooRemoteTransfer))
    {
      UnicodeString Label;
      if (FFileList->Count == 1)
      {
        UnicodeString FileName;
        if (!FToRemote) FileName = UnixExtractFileName(FFileList->Strings[0]);
          else FileName = ExtractFileName(FFileList->Strings[0]);
        Label = FMTLOAD(REMOTE_COPY_FILE, (FileName));
      }
      else
      {
        Label = FMTLOAD(REMOTE_COPY_FILES, (FFileList->Count));
      }

      DirectoryLabel->Caption = Label;
    }
    else
    {
      UnicodeString TransferStr =
        LoadStr(RemotePaths() ? COPY_COPY_TOREMOTE : COPY_COPY_TOLOCAL);
      // currently the copy dialog is shown when downloading to temp folder
      // only for drag&drop downloads, for we dare to display d&d specific prompt
      UnicodeString DirectionStr =
        LoadStr(((FOptions & coTemp) != 0) ? COPY_TODROP :
          (RemotePaths() ? COPY_TOREMOTE : COPY_TOLOCAL));

      if (FFileList->Count == 1)
      {
        UnicodeString FileName;
        if (!FToRemote) FileName = UnixExtractFileName(FFileList->Strings[0]);
          else FileName = ExtractFileName(FFileList->Strings[0]);
        DirectoryLabel->Caption = FMTLOAD((FMove ? MOVE_FILE : COPY_FILE),
          (TransferStr, FileName, DirectionStr));
      }
      else
      {
        DirectoryLabel->Caption = FMTLOAD((FMove ? MOVE_FILES : COPY_FILES),
          (TransferStr, FFileList->Count, DirectionStr));
      }
    }
  }

  UnicodeString ImageName;
  UnicodeString ACaption;
  if (!FMove)
  {
    if (!FToRemote && FLAGSET(FOutputOptions, cooRemoteTransfer))
    {
      ACaption = LoadStr(REMOTE_COPY_TITLE);
      ImageName = L"Duplicate";
    }
    else
    {
      if (RemotePaths())
      {
        ACaption = LoadStr(COPY_COPY_TOREMOTE_CAPTION);
        ImageName = L"Upload File";
      }
      else
      {
        ACaption = LoadStr(COPY_COPY_TOLOCAL_CAPTION);
        ImageName = L"Download File";
      }
    }
  }
  else
  {
    if (!FToRemote && FLAGSET(FOutputOptions, cooRemoteTransfer))
    {
      ACaption = LoadStr(COPY_MOVE_CAPTION);
      ImageName = L"Move To";
    }
    else
    {
      if (RemotePaths())
      {
        ACaption = LoadStr(COPY_MOVE_TOREMOTE_CAPTION);
        ImageName = L"Upload File Remove Original";
      }
      else
      {
        ACaption = LoadStr(COPY_MOVE_TOLOCAL_CAPTION);
        ImageName = L"Download File Remove Original";
      }
    }
  }

  Caption = FormatFormCaption(this, ACaption);

  LoadDialogImage(Image, ImageName);

  bool RemoteTransfer = FLAGSET(FOutputOptions, cooRemoteTransfer);
  DebugAssert(FLAGSET(FOptions, coAllowRemoteTransfer) || !RemoteTransfer);

  EnableControl(TransferSettingsButton, !RemoteTransfer);
  EnableControl(CopyParamGroup, !RemoteTransfer);
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::AdjustControls()
{
  RemoteDirectoryEdit->Visible = false;
  LocalDirectoryEdit->Visible = false;
  DirectoryEdit->Visible = FLAGCLEAR(FOptions, coTemp);
  EnableControl(DirectoryEdit, FLAGCLEAR(FOptions, coDisableDirectory));
  EnableControl(DirectoryLabel, DirectoryEdit->Enabled);
  EnableControl(LocalDirectoryBrowseButton, DirectoryEdit->Enabled);
  DirectoryLabel->FocusControl = DirectoryEdit;

  UnicodeString QueueLabel = LoadStr(COPY_BACKGROUND);
  if (FLAGCLEAR(FOptions, coNoQueue))
  {
    QueueLabel = FMTLOAD(COPY_QUEUE, (QueueLabel));
  }
  QueueCheck2->Caption = QueueLabel;

  AdjustTransferControls();

  LocalDirectoryBrowseButton->Visible = !FToRemote &&
    FLAGCLEAR(FOptions, coTemp);

  if (FLAGCLEAR(FOptions, coDoNotShowAgain))
  {
    NeverShowAgainCheck->Visible = false;
    ClientHeight = ClientHeight -
      (ShortCutHintPanel->Top - NeverShowAgainCheck->Top);
  }

  if (FLAGCLEAR(FOptions, coShortCutHint) || CustomWinConfiguration->CopyShortCutHintShown)
  {
    ShortCutHintPanel->Visible = false;
    ClientHeight = ClientHeight - ShortCutHintPanel->Height;
  }

  UpdateControls();
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::SetOutputOptions(int value)
{
  if (OutputOptions != value)
  {
    FSaveSettings = FLAGSET(value, cooSaveSettings);
    NeverShowAgainCheck->Checked = FLAGSET(value, cooDoNotShowAgain);
    FOutputOptions = (value & ~(cooDoNotShowAgain | cooSaveSettings));
  }
}
//---------------------------------------------------------------------------
int __fastcall TCopyDialog::GetOutputOptions()
{
  return FOutputOptions |
    FLAGMASK(FSaveSettings, cooSaveSettings) |
    FLAGMASK(NeverShowAgainCheck->Checked, cooDoNotShowAgain);
}
//---------------------------------------------------------------------------
THistoryComboBox * __fastcall TCopyDialog::GetDirectoryEdit()
{
  return FToRemote ? RemoteDirectoryEdit : LocalDirectoryEdit;
}
//---------------------------------------------------------------------------
bool __fastcall TCopyDialog::RemotePaths()
{
  return (FToRemote || FLAGSET(FOutputOptions, cooRemoteTransfer));
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TCopyDialog::GetFileMask()
{
  return ExtractFileName(DirectoryEdit->Text, RemotePaths());
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::SetParams(const TGUICopyParamType & value)
{
  FParams = value;
  FCopyParams = value;
  DirectoryEdit->Text = Directory + FParams.FileMask;
  QueueCheck2->Checked = FParams.Queue;
  UpdateControls();
}
//---------------------------------------------------------------------------
TGUICopyParamType __fastcall TCopyDialog::GetParams()
{
  // overwrites TCopyParamType fields only
  FParams = FCopyParams;
  FParams.FileMask = GetFileMask();
  FParams.Queue = QueueCheck2->Checked;
  return FParams;
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::SetDirectory(UnicodeString value)
{
  if (!value.IsEmpty())
  {
    value = RemotePaths() ?
      UnicodeString(UnixIncludeTrailingBackslash(value)) : IncludeTrailingBackslash(value);
  }
  DirectoryEdit->Text = value + GetFileMask();
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TCopyDialog::GetDirectory()
{
  DebugAssert(DirectoryEdit);

  UnicodeString Result = DirectoryEdit->Text;
  if (RemotePaths())
  {
    Result = UnixExtractFilePath(Result);
    if (!Result.IsEmpty())
    {
      Result = UnixIncludeTrailingBackslash(Result);
    }
  }
  else
  {
    Result = ExtractFilePath(Result);
    if (!Result.IsEmpty())
    {
      Result = IncludeTrailingBackslash(Result);
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::UpdateControls()
{
  if (!FToRemote && FLAGSET(FOptions, coAllowRemoteTransfer))
  {
    UnicodeString Directory = DirectoryEdit->Text;
    bool RemoteTransfer = (Directory.Pos(L"\\") == 0) && (Directory.Pos(L"/") > 0);
    if (RemoteTransfer != FLAGSET(FOutputOptions, cooRemoteTransfer))
    {
      FOutputOptions =
        (FOutputOptions & ~cooRemoteTransfer) |
        FLAGMASK(RemoteTransfer, cooRemoteTransfer);
      AdjustTransferControls();
    }
  }

  UnicodeString InfoStr = FCopyParams.GetInfoStr(L"; ", FCopyParamAttrs);
  SetLabelHintPopup(CopyParamLabel, InfoStr);

  bool RemoteTransfer = FLAGSET(FOutputOptions, cooRemoteTransfer);
  EnableControl(QueueCheck2,
    ((FOptions & (coDisableQueue | coTemp)) == 0) && !RemoteTransfer);

  TransferSettingsButton->Style =
    FLAGCLEAR(FOptions, coDoNotUsePresets) ?
      TCustomButton::bsSplitButton : TCustomButton::bsPushButton;
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::FormShow(TObject * /*Sender*/)
{
  DebugAssert(FFileList && (FFileList->Count > 0));
  if (DirectoryEdit->Enabled && DirectoryEdit->Visible)
  {
    ActiveControl = DirectoryEdit;
  }
  else
  {
    ActiveControl = OkButton;
  }
  UpdateControls();

  InstallPathWordBreakProc(RemoteDirectoryEdit);
  InstallPathWordBreakProc(LocalDirectoryEdit);
  // Does not work when set from a contructor
  ShortCutHintPanel->Color = Application->HintColor;
}
//---------------------------------------------------------------------------
bool __fastcall TCopyDialog::Execute()
{
  // at start assume that copy param is current preset
  FPreset = GUIConfiguration->CopyParamCurrent;
  DirectoryEdit->Items = CustomWinConfiguration->History[
    FToRemote ? L"RemoteTarget" : L"LocalTarget"];
  bool Result = (ShowModal() == DefaultResult(this));
  if (Result)
  {
    Configuration->BeginUpdate();
    try
    {
      if (FLAGSET(OutputOptions, cooSaveSettings) &&
          FLAGCLEAR(FOptions, coDisableSaveSettings))
      {
        GUIConfiguration->DefaultCopyParam = Params;
      }
      DirectoryEdit->SaveToHistory();
      CustomWinConfiguration->History[FToRemote ?
        L"RemoteTarget" : L"LocalTarget"] = DirectoryEdit->Items;

      if (FLAGSET(FOptions, coShortCutHint))
      {
        CustomWinConfiguration->CopyShortCutHintShown = true;
      }
    }
    __finally
    {
      Configuration->EndUpdate();
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::FormCloseQuery(TObject * /*Sender*/,
      bool &CanClose)
{
  if (ModalResult == DefaultResult(this))
  {
    if (!RemotePaths() && ((FOptions & coTemp) == 0))
    {
      UnicodeString Dir = Directory;
      UnicodeString Drive = ExtractFileDrive(Dir);
      if (!DirectoryExists(ApiPath(Dir)))
      {
        if (MessageDialog(MainInstructions(FMTLOAD(CREATE_LOCAL_DIRECTORY, (Dir))),
              qtConfirmation, qaOK | qaCancel, HELP_NONE) != qaCancel)
        {
          if (!ForceDirectories(ApiPath(Dir)))
          {
            SimpleErrorDialog(FMTLOAD(CREATE_LOCAL_DIR_ERROR, (Dir)));
            CanClose = false;
          }
        }
        else
        {
          CanClose = False;
        }
      }

      if (!CanClose)
      {
        DirectoryEdit->SelectAll();
        DirectoryEdit->SetFocus();
      }
    }

    if (CanClose && !IsFileNameMask(GetFileMask()) && (FFileList->Count > 1))
    {
      UnicodeString Message =
        FormatMultiFilesToOneConfirmation(DirectoryEdit->Text, RemotePaths());
      CanClose =
        (MessageDialog(Message, qtConfirmation, qaOK | qaCancel, HELP_NONE) != qaCancel);
    }

    if (CanClose)
    {
      ExitActiveControl(this);
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::LocalDirectoryBrowseButtonClick(
  TObject * /*Sender*/)
{
  DebugAssert(!FToRemote);
  UnicodeString ADirectory;
  // if we are duplicating, we have remote path there
  if (!RemotePaths())
  {
    ADirectory = Directory;
  }

  if (SelectDirectory(ADirectory, LoadStr(SELECT_LOCAL_DIRECTORY), true))
  {
    Directory = ADirectory;
    UpdateControls();
  }
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::ControlChange(TObject * /*Sender*/)
{
  UpdateControls();
  ResetSystemSettings(this);
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::TransferSettingsButtonClick(TObject * /*Sender*/)
{
  if (FLAGCLEAR(FOptions, coDoNotUsePresets) && !SupportsSplitButton())
  {
    CopyParamListPopup(CalculatePopupRect(TransferSettingsButton), 0);
  }
  else
  {
    CopyParamGroupClick(NULL);
  }
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::GenerateCode()
{
  TFilesSelected FilesSelected = FLAGSET(FOptions, coAllFiles) ? fsAll : fsList;
  DoGenerateTransferCodeDialog(FToRemote, FMove, FCopyParamAttrs, FSessionData, FilesSelected, FFileList, Directory, Params);
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::CopyParamClick(TObject * Sender)
{
  // Save including the preset-unspecific queue properties,
  // so that they are preserved when assigning back later
  TGUICopyParamType Param = Params;
  bool PrevSaveSettings = FSaveSettings;
  int Result = CopyParamListPopupClick(Sender, Param, FPreset, FCopyParamAttrs, &FSaveSettings);
  if (Result < 0)
  {
    if (DebugAlwaysTrue(Result == -cplGenerateCode))
    {
      GenerateCode();
    }
  }
  else
  {
    if (Result > 0)
    {
      Params = Param;
    }
    else
    {
      UpdateControls();
    }

    if (PrevSaveSettings && !FSaveSettings)
    {
      NeverShowAgainCheck->Checked = false;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::HelpButtonClick(TObject * /*Sender*/)
{
  FormHelp(this);
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::CopyParamGroupClick(TObject * /*Sender*/)
{
  if (CopyParamGroup->Enabled)
  {
    if (DoCopyParamCustomDialog(FCopyParams, FCopyParamAttrs))
    {
      UpdateControls();
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::CopyParamGroupContextPopup(TObject * /*Sender*/,
  TPoint & MousePos, bool & Handled)
{
  if (FLAGCLEAR(FOptions, coDoNotUsePresets))
  {
    CopyParamListPopup(CalculatePopupRect(CopyParamGroup, MousePos), cplCustomizeDefault);
    Handled = true;
  }
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::CopyParamListPopup(TRect R, int AdditionalOptions)
{
  bool RemoteTransfer = FLAGSET(FOutputOptions, cooRemoteTransfer);

  ::CopyParamListPopup(R, FPresetsMenu,
    FCopyParams, FPreset, CopyParamClick,
    cplCustomize | AdditionalOptions |
      FLAGMASK(
          FLAGCLEAR(FOptions, coDisableSaveSettings) && !RemoteTransfer,
        cplSaveSettings) |
      FLAGMASK(FLAGCLEAR(FOutputOptions, cooRemoteTransfer) && FLAGCLEAR(FOptions, coTemp), cplGenerateCode),
    FCopyParamAttrs,
    FSaveSettings);
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::TransferSettingsButtonDropDownClick(TObject * /*Sender*/)
{
  CopyParamListPopup(CalculatePopupRect(TransferSettingsButton), cplCustomizeDefault);
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::NeverShowAgainCheckClick(TObject * /*Sender*/)
{
  FSaveSettings = NeverShowAgainCheck->Checked;
  UpdateControls();
}
//---------------------------------------------------------------------------
void __fastcall TCopyDialog::ShortCutHintLabelClick(TObject * /*Sender*/)
{
  DoPreferencesDialog(pmCommander);
}
//---------------------------------------------------------------------------

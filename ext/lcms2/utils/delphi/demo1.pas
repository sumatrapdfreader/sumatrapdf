unit demo1;

interface

uses
  Windows, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ExtCtrls, StdCtrls, ExtDlgs, lcms2dll, ComCtrls;

type
  TForm1 = class(TForm)

    Image1: TImage;
    Image2: TImage;
    Panel1: TPanel;
    Splitter1: TSplitter;
    Button2: TButton;
    ComboBoxInput: TComboBox;
    ComboBoxOutput: TComboBox;
    Label1: TLabel;
    Label2: TLabel;
    WBCompensation: TCheckBox;
    NoTransform: TCheckBox;
    RadioGroup1: TRadioGroup;
    OpenPictureDialog1: TOpenPictureDialog;
    Button1: TButton;
    ProgressBar1: TProgressBar;
    ComboBoxIntent: TComboBox;
    Label3: TLabel;
    Button3: TButton;
    Button4: TButton;
    OpenDialog1: TOpenDialog;
    Label4: TLabel;
    ScrollBar1: TScrollBar;

    procedure Button2Click(Sender: TObject);
    procedure Button1Click(Sender: TObject);
    procedure Button3Click(Sender: TObject);
    procedure Button4Click(Sender: TObject);
    procedure ComboBoxIntentChange(Sender: TObject);
    procedure ScrollBar1Change(Sender: TObject);
  private
    { Private declarations }
    function ComputeFlags: DWORD;

  public
    constructor Create(Owner: TComponent); Override;
    { Public declarations }
  end;

var
  Form1: TForm1;

implementation

{$R *.DFM}

CONST
  IS_INPUT = $1;
  IS_DISPLAY = $2;
  IS_COLORSPACE = $4;
  IS_OUTPUT = $8;
  IS_ABSTRACT = $10;

VAR
   IntentCodes: array [0 .. 20] of cmsUInt32Number;

FUNCTION InSignatures(Signature: cmsProfileClassSignature;  dwFlags: DWORD): Boolean;
BEGIN

  if (((dwFlags AND IS_DISPLAY) <> 0) AND (Signature = cmsSigDisplayClass)) then
    InSignatures := TRUE
  else if (((dwFlags AND IS_OUTPUT) <> 0) AND (Signature = cmsSigOutputClass))
    then
    InSignatures := TRUE
  else if (((dwFlags AND IS_INPUT) <> 0) AND (Signature = cmsSigInputClass))
    then
    InSignatures := TRUE
  else if (((dwFlags AND IS_COLORSPACE) <> 0) AND
      (Signature = cmsSigColorSpaceClass)) then
    InSignatures := TRUE
  else if (((dwFlags AND IS_ABSTRACT) <> 0) AND
      (Signature = cmsSigAbstractClass)) then
    InSignatures := TRUE
  else
    InSignatures := FALSE
END;

PROCEDURE FillCombo(var Combo: TComboBox; Signatures: DWORD);
var
  Files, Descriptions: TStringList;
  Found: Integer;
  SearchRec: TSearchRec;
  Path, Profile: String;
  Dir: ARRAY [0 .. 1024] OF Char;
  hProfile: cmsHPROFILE;
  Descrip: array [0 .. 256] of Char;
begin
  Files := TStringList.Create;
  Descriptions := TStringList.Create;
  GetSystemDirectory(Dir, 1023);
  Path := String(Dir) + '\SPOOL\DRIVERS\COLOR\';
  Found := FindFirst(Path + '*.ic?', faAnyFile, SearchRec);
  while Found = 0 do
  begin
    Profile := Path + SearchRec.Name;
    hProfile := cmsOpenProfileFromFile(PAnsiChar(AnsiString(Profile)), 'r');
    if (hProfile <> NIL) THEN
    begin

      if ((cmsGetColorSpace(hProfile) = cmsSigRgbData) AND InSignatures
          (cmsGetDeviceClass(hProfile), Signatures)) then
      begin
        cmsGetProfileInfo(hProfile, cmsInfoDescription, 'EN', 'us', Descrip,
          256);
        Descriptions.Add(Descrip);
        Files.Add(Profile);
      end;
      cmsCloseProfile(hProfile);
    end;

    Found := FindNext(SearchRec);

  end;
  FindClose(SearchRec);
  Combo.Items := Descriptions;
  Combo.Tag := Integer(Files);
end;

// A rather simple Logger... note the "cdecl" convention
PROCEDURE ErrorLogger(ContextID: cmsContext; ErrorCode: cmsUInt32Number;
  Text: PAnsiChar); Cdecl;
begin
  MessageBox(0, PWideChar(WideString(Text)), 'Something is going wrong...',
    MB_OK OR MB_ICONWARNING or MB_TASKMODAL);
end;

constructor TForm1.Create(Owner: TComponent);
var
  IntentNames: array [0 .. 20] of PAnsiChar;
  i, n: Integer;
begin
  inherited Create(Owner);

   // Set the logger
  cmsSetLogErrorHandler(ErrorLogger);

  ScrollBar1.Min := 0;
  ScrollBar1.Max := 100;

  FillCombo(ComboBoxInput, IS_INPUT OR IS_COLORSPACE OR IS_DISPLAY);
  FillCombo(ComboBoxOutput, $FFFF  );


  // Get the supported intents
  n := cmsGetSupportedIntents(20, @IntentCodes, @IntentNames);


  ComboBoxIntent.Items.BeginUpdate;
  ComboBoxIntent.Items.Clear;
  for i:= 0 TO n - 1 DO
    ComboBoxIntent.Items.Add(String(IntentNames[i]));

  ComboBoxIntent.ItemIndex := 0;
  ComboBoxIntent.Items.EndUpdate;
end;



procedure TForm1.ScrollBar1Change(Sender: TObject);
var d: Integer;
    s: String;
begin
     d := ScrollBar1.Position;
     Str(d, s);
     Label4.Caption := 'Adaptation state '+s + '% (Abs. col only)';
end;

procedure TForm1.Button2Click(Sender: TObject);
begin
  if OpenPictureDialog1.Execute then
  begin
    Image1.Picture.LoadFromFile(OpenPictureDialog1.FileName);
    Image1.Picture.Bitmap.PixelFormat := pf24bit;

    Image2.Picture.LoadFromFile(OpenPictureDialog1.FileName);
    Image2.Picture.Bitmap.PixelFormat := pf24bit;

  end
end;

function SelectedFile(var Combo: TComboBox): string;
var
  List: TStringList;
  n: Integer;
begin

  List := TStringList(Combo.Tag);
  n := Combo.ItemIndex;
  if (n >= 0) then
    SelectedFile := List.Strings[n]
  else
    SelectedFile := Combo.Text;
end;

procedure TForm1.ComboBoxIntentChange(Sender: TObject);
begin
   ScrollBar1.Enabled := (ComboBoxIntent.itemIndex = 3);
end;

function TForm1.ComputeFlags: DWORD;
var
  dwFlags: DWORD;
begin
  dwFlags := 0;
  if (WBCompensation.Checked) then
  begin
    dwFlags := dwFlags OR cmsFLAGS_BLACKPOINTCOMPENSATION
  end;

  if (NoTransform.Checked) then
  begin
    dwFlags := dwFlags OR cmsFLAGS_NULLTRANSFORM
  end;

  case RadioGroup1.ItemIndex of
    0:
      dwFlags := dwFlags OR cmsFLAGS_NOOPTIMIZE;
    1:
      dwFlags := dwFlags OR cmsFLAGS_HIGHRESPRECALC;
    3:
      dwFlags := dwFlags OR cmsFLAGS_LOWRESPRECALC;
  end;

  ComputeFlags := dwFlags
end;

procedure TForm1.Button1Click(Sender: TObject);
var
  Source, Dest: String;
  hSrc, hDest: cmsHPROFILE;
  xform: cmsHTRANSFORM;
  i, PicW, PicH: Integer;
  Intent: Integer;
  dwFlags: DWORD;
begin

  Source := SelectedFile(ComboBoxInput);
  Dest := SelectedFile(ComboBoxOutput);

  dwFlags := ComputeFlags;

  Intent := IntentCodes[ComboBoxIntent.ItemIndex];

  cmsSetAdaptationState(  ScrollBar1.Position / 100.0 );

  if (Source <> '') AND (Dest <> '') then
  begin
    hSrc := cmsOpenProfileFromFile(PAnsiChar(AnsiString(Source)), 'r');
    hDest := cmsOpenProfileFromFile(PAnsiChar(AnsiString(Dest)), 'r');

    if (hSrc <> Nil) and (hDest <> Nil) then
    begin
      xform := cmsCreateTransform(hSrc, TYPE_BGR_8, hDest, TYPE_BGR_8, Intent,
        dwFlags);
    end
    else
    begin
      xform := nil;
    end;

    if hSrc <> nil then
    begin
      cmsCloseProfile(hSrc);
    end;

    if hDest <> Nil then
    begin
      cmsCloseProfile(hDest);
    end;

    if (xform <> nil) then
    begin

      PicW := Image2.Picture.width;
      PicH := Image2.Picture.height;
      ProgressBar1.Min := 0;
      ProgressBar1.Max := PicH;
      ProgressBar1.Step := 1;

      for i := 0 TO (PicH - 1) do
      begin
        if ((i MOD 100) = 0) then
          ProgressBar1.Position := i;

        cmsDoTransform(xform, Image1.Picture.Bitmap.Scanline[i],
          Image2.Picture.Bitmap.Scanline[i], PicW);

      end;
      ProgressBar1.Position := PicH;

      cmsDeleteTransform(xform);

    end;

    Image2.Repaint;
    ProgressBar1.Position := 0;
  end
end;

procedure TForm1.Button3Click(Sender: TObject);
begin
  if OpenDialog1.Execute then
    ComboBoxInput.Text := OpenDialog1.FileName;
end;

procedure TForm1.Button4Click(Sender: TObject);
begin
  if OpenDialog1.Execute then
    ComboBoxOutput.Text := OpenDialog1.FileName;
end;

end.

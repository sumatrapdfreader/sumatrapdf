#!/usr/bin/php
<?php

if (!function_exists('file_put_contents')) {
    function file_put_contents($filename, $data, $lockflag) {
        $f = @fopen($filename, 'w');
        if (!$f) {
            return false;
        } else {
            $bytes = fwrite($f, $data);
            fclose($f);
            return $bytes;
        }
    }
}

function convertquotes($in) 
{
  $ret = "";
  $qs= 0;
  for ($x=0;$x<strlen($in); $x++)
  {
    if ($in[$x] == '"')
    {
      if (!$qs) 
      {
        $qs=1;
      }
      else
      {
        $qs=0;
        if ($x < strlen($in) && $in[$x+1] == '"') 
        {
          $ret .= "\\";
          continue;
        }
      }
    }
    $ret .= $in[$x];
  }
  return $ret;
}

function utf_fgets($fp, &$state)
{
  if ($state==0)
  {
    $state = -1;
    $s = fread($fp,2);
    if (ord($s[0])==0xff && ord($s[1]) == 0xfe) $state=1; // UTF-16LE
    else if (ord($s[0])==0xfe && ord($s[1]) == 0xff) $state=2; // UTF-16BE
    else if (ord($s[0])==0xef && ord($s[1]) == 0xbb)
    {
      $s2 = fread($fp,1);
      if (ord($s2[0]) != 0xbf) return $s . $s2 . fgets($fp,4096);
    }
  }
  if ($state < 0) return fgets($fp,4096);

  $ret = "";
  while ($s = fread($fp,2))
  {
    if ($state == 1) $s = ord($s[0]) | (ord($s[1])<<8);
    else $s = ord($s[1]) | (ord($s[0])<<8);
    if ($s < 128) 
    {
      $ret .= chr($s);
      if ($s == ord("\n")) break;
    }
    else if ($s <= 0x7ff) $ret .= chr(0xc0 | (($s>>6)&0x1f)) . chr(0x80 | ($s&0x3f));
    else $ret .= chr(0xe0 | (($s>>12)&0xf)) . chr(0x80 | (($s)>>6)&0x3f) . chr(0x80 | ($s&0x3f));
  }

  return $ret;
}

function swell_rc2cpp_dialog($fp) // returns array with ["data"] and optionally ["error"]
{
  fseek($fp,0,SEEK_SET);
  $utf_state=0;
  $errstr="";
  $retstr = "";

  $retstr .= '#ifndef SWELL_DLG_SCALE_AUTOGEN' . "\n";
  $retstr .= '#ifdef __APPLE__' . "\n";
  $retstr .= '  #define SWELL_DLG_SCALE_AUTOGEN 1.7' . "\n";
  $retstr .= '#else' . "\n";
  $retstr .= '  #define SWELL_DLG_SCALE_AUTOGEN 1.9' . "\n";
  $retstr .= '#endif' . "\n";
  $retstr .= '#endif' . "\n";
  $retstr .= '#ifndef SWELL_DLG_FLAGS_AUTOGEN' . "\n";
  $retstr .= '#define SWELL_DLG_FLAGS_AUTOGEN SWELL_DLG_WS_FLIPPED|SWELL_DLG_WS_NOAUTOSIZE' . "\n";
  $retstr .= "#endif\n";
  $retstr .= "\n";

  $dlg_state=0; // 1 = before BEGIN, 2=after BEGIN

  $dlg_name="";
  $dlg_size_w=0;
  $dlg_size_h=0;
  $dlg_title = "";
  $dlg_styles = "SWELL_DLG_FLAGS_AUTOGEN";
  $dlg_contents="";

  $next_line="";
  for (;;) 
  {
    if ($next_line != "") { $x=$next_line; $next_line =""; }
    else if (!($x=utf_fgets($fp,$utf_state))) break;
    $x = convertquotes($x);

    $y=trim($x);
    if ($dlg_state>=2) 
    {
      if (preg_match("/^LTEXT(.*), *WS_EX_RIGHT$/",$y,$match)) $y = "RTEXT" . $match[1];
      $dlg_contents .= $y . "\n";
      if ($y == "END")
      {
        if ($dlg_state==2) $dlg_styles.="|SWELL_DLG_WS_OPAQUE";
        $retstr .= "#ifndef SET_$dlg_name" . "_SCALE\n";
        $retstr .= "#define SET_$dlg_name" . "_SCALE SWELL_DLG_SCALE_AUTOGEN\n";
        $retstr .= "#endif\n";
        $retstr .= "#ifndef SET_$dlg_name" . "_STYLE\n";
        $retstr .= "#define SET_$dlg_name" . "_STYLE $dlg_styles\n";
        $retstr .= "#endif\n";
        $retstr .= "SWELL_DEFINE_DIALOG_RESOURCE_BEGIN($dlg_name,SET_$dlg_name" . "_STYLE,\"$dlg_title\",$dlg_size_w,$dlg_size_h,SET_$dlg_name" . "_SCALE)\n";
        $dlg_contents=str_replace("NOT WS_VISIBLE","SWELL_NOT_WS_VISIBLE",$dlg_contents);
        $dlg_contents=str_replace("NOT\nWS_VISIBLE","SWELL_NOT_WS_VISIBLE",$dlg_contents);
        $dlg_contents=str_replace("NOT \nWS_VISIBLE","SWELL_NOT_WS_VISIBLE",$dlg_contents);
        $retstr .= $dlg_contents;
        $retstr .= "SWELL_DEFINE_DIALOG_RESOURCE_END($dlg_name)\n\n\n";
        $dlg_state=0;
      }
      else if (strlen($y)>1) $dlg_state=3;
    }
    else 
    {
      $parms = explode(" ", $y);
      if (count($parms) > 0)
      {
        if ($dlg_state == 0)
        {
          // if (substr($parms[0],0,8) == "IDD_PREF") 
          if (count($parms)>4 && ($parms[1] == 'DIALOGEX'||$parms[1] == 'DIALOG'))
          {
            $dlg_name=$parms[0];
            $rdidx = 2;
            if ($parms[$rdidx] == 'DISCARDABLE') $rdidx++;
            while ($parms[$rdidx] == "" && $rdidx < count($parms)) $rdidx++;
            $rdidx  += 2;
            $dlg_size_w = str_replace(",","",$parms[$rdidx++]);
            $dlg_size_h = str_replace(",","",$parms[$rdidx++]);
            if (count($parms) >= $rdidx && $dlg_size_w != "" && $dlg_size_h != "")
            {
              $dlg_title=""; 
              $dlg_styles="SWELL_DLG_FLAGS_AUTOGEN"; 
              $dlg_contents="";
              $dlg_state=1;
            }
            else  $errstr .= "WARNING: corrupted $dlg_name resource\n";
          }
        }
        else if ($dlg_state == 1)
        {
          if ($parms[0] == "BEGIN")
          {
            $dlg_state=2;
            $dlg_contents = $y ."\n";
          }
          else
          {
            if ($parms[0] == "CAPTION") 
            {
             $dlg_title = str_replace("\"","",trim(substr($y,8)));
            }
            else if ($parms[0] == "STYLE" || $parms[0] == "EXSTYLE")
            { 
              $rep=0;
              for (;;) 
              {
                $next_line = utf_fgets($fp,$utf_state);
                if (!($next_line )) { $next_line=""; break; }
                if (substr($next_line,0,1)==" " || substr($next_line,0,1)=="\t")
                {
                  $y .= " " . trim(convertquotes($next_line));
                  $rep++;
                  $next_line="";
                }
                else break;
              }
              if ($rep) $parms = explode(" ", $y);
              $opmode=0;
              $rdidx=1;
              while ($rdidx < count($parms))
              {
                if ($parms[$rdidx] == '|') { $opmode=0; }
                else if ($parms[$rdidx] == 'NOT') { $opmode=1; }
                else if ($parms[$rdidx] == 'WS_CHILD') 
                {
                  if (!$opmode) $dlg_styles .= "|SWELL_DLG_WS_CHILD";
                }
                else if ($parms[$rdidx] == 'WS_THICKFRAME') 
                {
                  if (!$opmode) $dlg_styles .= "|SWELL_DLG_WS_RESIZABLE";
                }
                else if ($parms[$rdidx] == 'WS_EX_ACCEPTFILES') 
                {
                  if (!$opmode) $dlg_styles .= "|SWELL_DLG_WS_DROPTARGET";
                }
                else if ($parms[$rdidx] == 'WS_CLIPSIBLINGS') 
                {
                  if (!$opmode) $dlg_styles .= "|WS_CLIPSIBLINGS";
                }
                else $opmode=0;
                $rdidx++;
              }
            }
          }
        } 
      }
    }
  }
  if ($dlg_state != 0)
     $errstr .= "WARNING: there may have been a truncated  dialog resource ($dlg_name)\n";

  $retstr .= "\n//EOF\n\n";
  $rv = array();
  $rv["data"] = $retstr;
  $rv["error"] = $errstr;
  return $rv;
}

function swell_rc2cpp_menu($fp) // returns array with ["data"] and optionally ["error"]
{
  $retstr="";
  $errstr="";

  fseek($fp,0,SEEK_SET);
  $utf_state=0;

  $menu_symbol="";
  $menu_depth=0;
  while (($x=utf_fgets($fp,$utf_state)))
  {
    $x = convertquotes($x);

    $y=trim($x);
    if ($menu_symbol == "")
    {
      $parms = explode(" ", $y);
      $tok = "MENU";
      if (count($parms) >= 2 && $parms[1] == $tok)
      {
        $menu_symbol = $parms[0];
        $menu_depth=0;
        $retstr .= "SWELL_DEFINE_MENU_RESOURCE_BEGIN($menu_symbol)\n";
      }
    }
    else
    { 
      if ($y == "END") 
      {
        $menu_depth-=1;
        if ($menu_depth == 0)
        {
          $retstr .= "SWELL_DEFINE_MENU_RESOURCE_END($menu_symbol)\n\n\n";
        }
        if ($menu_depth < 1) $menu_symbol="";
      }
      if ($menu_depth>0) 
      {
        if (substr($y,-strlen(", HELP")) == ", HELP") 
        {
          $x=substr(rtrim($x),0,-strlen(", HELP")) . "\n";
        }
        $retstr .= $x;
      }
      if ($y == "BEGIN") $menu_depth+=1;
    }
  }

  $retstr .= "\n//EOF\n\n";
  $rv = array();
  $rv["data"] = $retstr;
  $rv["error"] = $errstr;

  return $rv;
}

if (count($argv)<2) die("usage: mac_resgen.php [--force] file.rc ...\n");

$x=1;
$forcemode = 0;
if ($argv[$x] == "--force") { $forcemode=1; $x++; }

$lp = dirname(__FILE__);
$proc=0;
$skipped=0;
$err=0;
$quiet=0;
for (; $x < count($argv); $x ++)
{
   $srcfn = $argv[$x];
   if ($srcfn == "--quiet")
   {
     $quiet = 1;
     continue;
   }
   if (!stristr($srcfn,".rc") || !($fp = @fopen($srcfn,"r")))
   {
      $err++;
      echo "$srcfn: not valid or not found!\n";
      continue;
   }
   $ofnmenu = $srcfn . "_mac_menu";
   $ofndlg = $srcfn . "_mac_dlg";

   $res = swell_rc2cpp_dialog($fp);
   $res2 = swell_rc2cpp_menu($fp);
   fclose($fp);
   if ($res["error"] != "" || $res2["error"] != "")
   {
     $err++;
     echo "$srcfn: error";
     if ($res["error"] != "") echo " dialog: " . $res["error"];
     if ($res2["error"] != "") echo " menu: " . $res2["error"];
     echo "\n";
     continue;
   }
   $f="";
   if ($forcemode || !file_exists($ofndlg) || file_get_contents($ofndlg) != $res["data"])
   {
     $f .= "dlg updated";
     if (!file_put_contents($ofndlg,$res["data"],LOCK_EX)) { echo "error writing $ofndlg\n"; $err++; }
   }
   if ($forcemode || !file_exists($ofnmenu) || file_get_contents($ofnmenu) != $res2["data"])
   {
     if ($f != "") $f .= ", ";
     $f .= "menu updated";
     if (!file_put_contents($ofnmenu,$res2["data"],LOCK_EX)) { echo "error writing $ofnmenu\n"; $err++; }
   }

   if ($f != "") 
   {
     $proc++; 
   }
   else 
   {
     $skipped++;
     $f = "skipped";
   }

   if (!$quiet) echo "$srcfn: $f\n";
}
if (!$quiet || $proc || $err) echo "processed $proc, skipped $skipped, error $err\n";

?>

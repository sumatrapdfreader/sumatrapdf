<?php

if (isset($_SERVER['REMOTE_ADDR'])) die("no perms");

require("stream-config.php");

$cfgname = $argc>1 ? $argv[1] : $config_default_stream;
if (!isset($config_streams[$cfgname])) die("unknown stream name\n");
$cfg = $config_streams[$cfgname];

$fp = @fopen($cfg["path"] . "/$cfgname" ."_session.txt","rb");
if (!$fp) die("error opening session\n");
$x = fgets($fp,4096);
fclose($fp);
if ($x === FALSE) die("error reading session\n");
$x = trim($x);
if ($x == "") die("error reading session 2\n");

$fp = @fopen($cfg["path"] . "/$x.log","r");
if (!$fp) die("error opening input\n");

$lastcnt = -1;
$maxcnt=0;
$list=array();
for (;;)
{
  $x = fgets($fp,4096);
  if ($x === FALSE)
  {
    $cnt=0;
    $now = time();
    foreach ($list as $x => $v) if ($now < $v+45) $cnt++;
    if ($cnt > $maxcnt) $maxcnt = $cnt;
    if ($cnt != $lastcnt)
    {
      echo "listeners $cnt (max $maxcnt)\n";
      $lastcnt=$cnt;
    }
    sleep(1);
    clearstatcache();
    fseek($fp,ftell($fp),SEEK_SET);
  }
  else
  {
    $x = explode(" ", trim($x));
    if ($x[0] == "CONNECT" || $x[0] == "UPDATE") $list[$x[1]] = (int)$x[2];
    else if ($x[0] == "DISCONNECT") $list[$x[1]] = 0;
  }
}

?>

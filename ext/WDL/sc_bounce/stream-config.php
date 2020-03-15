<?php

// some path that the server can write in (or create)
$config_default_stream = "test"; // if no stream specified, use this one
$config_session_timeout = 120; // seconds of inactivity = new session 
$config_presend = 32000; // presend bytes on listener connect
$config_keep_files = true; // keep old recordings

// allowed servers, with their name, password, write path, optional listen password
$config_streams = array(
  "test" => array("password" => "somePassword", 
                  "path" => "test_files",   // optional (stream name used if not specified)
                  "session_timeout" => 120, // timeout in seconds ($config_session_timeout)
                  "presend" => 32000,       // pre-send bytes ($config_presend)
                  "keep_files" => true,
                  "listen_password" => "",),

);


?>

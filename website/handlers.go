package main

var (
	html404 = `
<!doctype html>
<html>
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
  <title>SumatraPDF Reader - page not found</title>
	<link rel="stylesheet" href="sumatra.css" type="text/css">
</head>
<body>
  <div style="margin-top:64px; margin-left:auto; margin-right:auto; max-width:800px">
    <p style="color:red">Page <tt>${url}</tt> doesn't exist!</p>
    <p>Try:
      <ul>
        <li><a href="/">Home</a></li>
      </ul>
    </p>
  </div>
</body>
</html>
`
)

/*
	mux.HandleFunc("/dl/", withAnalyticsLogging(handleDl))
*/

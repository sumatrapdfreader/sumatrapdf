INPUT=docs
OUTPUT=../web/mupdf.com/docs

for I in $(find $INPUT/examples -type f)
do
	B=$(echo $I | sed s,$INPUT/,,)
	O=$OUTPUT/$B
	cp $I $O
done

for I in $(find $INPUT -name '*.html')
do
	B=$(echo $I | sed s,$INPUT/,,)
	O=$OUTPUT/$B

	TITLE=$(cat $I | grep '<title>' | sed 's,</*title>,,g')

	ROOT=https://www.mupdf.com

	echo Processing $O "($TITLE)"

	sed '/<article>/,/<\/article>/p;d' < $I > temp.body

	cat >temp.head <<EOF
<!DOCTYPE html>
<html>
<head>
	<!-- Global site tag (gtag.js) - Google Analytics -->
	<script async src="https://www.googletagmanager.com/gtag/js?id=UA-54391264-6">
	</script>
	<script>
		window.dataLayer = window.dataLayer || [];
		function gtag(){dataLayer.push(arguments);}
		gtag('js', new Date());
		gtag('config', 'UA-54391264-6');
	</script>
	<meta charset="UTF-8">
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link href="https://fonts.googleapis.com/css?family=Source+Sans+Pro" rel="stylesheet">
	<link rel="shortcut icon" type="image/png" href="$ROOT/images/favicon.png">
	<link href="$ROOT/style.css" rel="stylesheet" type="text/css">
	<title>$TITLE</title>
</head>

<body>
	<div class="header">
		<div class="row">
			<div class="col-lt-6 logo">
				<a href="$ROOT/index.html"><img src="$ROOT/images/MuPDFgreek_logo.png" width="90" height="119" alt="MuPDF Logo"></a>
			</div>
			<div class="col-6">
				<div class="row">
					<div class="artifexlogo">
						<a href="https://www.artifex.com/" target="_blank"><img src="$ROOT/images/Artifex_logo.png" width="194" height="40" alt="Artifex Logo"></a>
					</div>
					<div class="col-12">
						<div class="button button1">
							<a href="https://www.artifex.com/contact-us/" title="Contact Us" target="_blank">Contact Us</a>
						</div>
						<div class="button button2 hidden-xs">
							<a href="$ROOT/downloads/index.html">Download</a>
						</div>
					</div>
				</div>
			</div>
		</div>
	</div>

	<div class="banner">
		<div class="row">
			<div class="col-12">
				$TITLE
			</div>
		</div>
	</div>

	<div class="main">
		<div class="row">
			<div id="sidebar">
				<div class="sidebar-item"></div>
				<div class="col-2 leftnav">
					<ul>
						<li> <a href="$ROOT/index.html">Home</a> </li>
						<li> <a href="$ROOT/release_history.html">Release History</a> </li>
						<li> <a href="$ROOT/docs/index.html">Documentation</a> </li>
						<li> <a href="$ROOT/downloads/index.html">Downloads</a> </li>
						<li> <a href="$ROOT/license.html">Licensing</a> </li>
						<li> <a href="http://git.ghostscript.com/?p=mupdf.git;a=summary" target="_blank">Source</a> </li>
						<li> <a href="http://bugs.ghostscript.com/" target="_blank">Bugs</a> </li>
					</ul>
				</div>
			</div>
			<div class="col-10 page">
<!-- BEGIN ARTICLE -->
EOF

cat >temp.foot <<EOF
<!-- END ARTICLE -->
			</div>
		</div>
	</div>
	<div class="footer">
		<div class="row">
			<div class="col-7 footleft">
				<ul>
					<li> <a href="https://artifex.com/contact-us/" target="new">CONTACT US</a> </li>
					<li> <a href="https://artifex.com/about-us/" target="new">ABOUT</a> </li>
					<li> <a href="../security.html" target="_blank">SECURITY</a> </li>
				</ul>
			</div>
			<div class="col-1 footcenter">
				<ul>
					<li> <a href="https://artifex.com/support/" target="new">SUPPORT</a> </li>
					<li> <a href="https://artifex.com/blog/artifex/" target="new">BLOG</a> </li>
					<li> <a href="https://artifex.com/privacy-policy/" target="new">PRIVACY</a> </li>
				</ul>
			</div>
			<div class="col-ft-3 footright">
				<img src="../images/Artifex_logo.png" width="194" height="40" alt=""><br>
				&copy; Copyright 2019 Artifex Software, Inc.<br>
				All rights reserved.
			</div>
		</div>
	</div>
	<script src="https://cdnjs.cloudflare.com/ajax/libs/jquery/2.1.3/jquery.min.js">
	</script>
	<script src="../index.js">
	</script>
</body>
</html>
EOF

	cat temp.head temp.body temp.foot > $O
done

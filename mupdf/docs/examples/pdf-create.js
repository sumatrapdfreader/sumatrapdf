// Create a PDF from scratch using helper functions.

// This example creates a new PDF file from scratch, using helper
// functions to create resources and page objects.
// This assumes a basic working knowledge of the PDF file format.

// Create a new empty document with no pages.
var pdf = new PDFDocument()

// Load built-in font and create WinAnsi encoded simple font resource.
var font = pdf.addSimpleFont(new Font("Times-Roman"))

// Load PNG file and create image resource.
var image = pdf.addImage(new Image("example.png"))

// Create resource dictionary.
var resources = pdf.addObject({
	Font: { Tm: font },
	XObject: { Im0: image },
})

// Create content stream data.
var contents =
	"10 10 280 330 re s\n" +
	"q 200 0 0 200 50 100 cm /Im0 Do Q\n" +
	"BT /Tm 16 Tf 50 50 TD (Hello, world!) Tj ET\n"

// Create a new page object.
var page = pdf.addPage([0,0,300,350], 0, resources, contents)

// Insert page object at the end of the document.
pdf.insertPage(-1, page)

// Save the document to file.
pdf.save("out.pdf", "pretty,ascii,compress-images,compress-fonts")

// Create a PDF from scratch.

// This example creates a new PDF file from scratch, using only the low level APIs.
// This assumes a basic working knowledge of the PDF file format.

// Create a new empty document with no pages.
var pdf = new PDFDocument()

// Create and add a font resource.
var font = pdf.addObject({
	Type: "Font",
	Subtype: "Type1",
	Encoding: "WinAnsiEncoding",
	BaseFont: "Times-Roman",
})

// Create and add an image resource:
var image = pdf.addRawStream(
	// The raw stream contents, hex encoded to match the Filter entry:
	"004488CCEEBB7733>",
	// The image object dictionary:
	{
		Type: "XObject",
		Subtype: "Image",
		Width: 4,
		Height: 2,
		BitsPerComponent: 8,
		ColorSpace: "DeviceGray",
		Filter: "ASCIIHexDecode",
	}
);

// Create resource dictionary.
var resources = pdf.addObject({
	Font: { Tm: font },
	XObject: { Im0: image },
})

// Create content stream.
var buffer = new Buffer()
buffer.writeLine("10 10 280 330 re s")
buffer.writeLine("q 200 0 0 200 50 100 cm /Im0 Do Q")
buffer.writeLine("BT /Tm 16 Tf 50 50 TD (Hello, world!) Tj ET")
var contents = pdf.addStream(buffer)

// Create page object.
var page = pdf.addObject({
	Type: "Page",
	MediaBox: [0,0,300,350],
	Contents: contents,
	Resources: resources,
})

// Insert page object into page tree.
var pagetree = pdf.getTrailer().Root.Pages
pagetree.Count = 1
pagetree.Kids = [ page ]
page.Parent = pagetree

// Save the document.
pdf.save("out.pdf")

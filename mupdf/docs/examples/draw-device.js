// Use device interface to draw some graphics and save as a PNG.

var font = new Font("Times-Roman");
var image = new Image("example.png");
var path, text;

var pixmap = new Pixmap(DeviceRGB, [0,0,500,600], false);
pixmap.clear(255);
var device = new DrawDevice(Identity, pixmap);
var transform = [2,0,0,2,0,0]
{
	text = new Text();
	{
		text.showString(font, [16,0,0,-16,100,30], "Hello, world!");
		text.showString(font, [0,16,16,0,15,100], "Hello, world!");
	}
	device.fillText(text, transform, DeviceGray, [0], 1);

	path = new Path();
	{
		path.moveTo(10, 10);
		path.lineTo(90, 10);
		path.lineTo(90, 90);
		path.lineTo(10, 90);
		path.closePath();
	}
	device.fillPath(path, false, transform, DeviceRGB, [1,0,0], 1);
	device.strokePath(path, {dashes:[5,10], lineWidth:3, lineCap:'Round'}, transform, DeviceRGB, [0,0,0], 1);

	path = new Path();
	{
		path.moveTo(100,100);
		path.curveTo(150,100, 200,150, 200,200);
		path.curveTo(200,300, 0,300, 100,100);
		path.closePath();
	}
	device.clipPath(path, true, transform);
	{
		device.fillImage(image, Concat(transform, [300,0,0,300,0,0]), 1);
	}
	device.popClip();
}
device.close();

pixmap.saveAsPNG("out.png");

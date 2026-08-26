import http.client
import urllib.parse
conn = http.client.HTTPSConnection('lens.google.com')
data = b'--b\r\nContent-Disposition: form-data; name="encoded_image"; filename="i.png"\r\nContent-Type: image/png\r\n\r\n' + open('gfx/SumatraPDF-smaller.ico', 'rb').read() + b'\r\n--b--\r\n'
conn.request('POST', '/v3/upload', body=data, headers={'Content-Type':'multipart/form-data; boundary=b'})
res = conn.getresponse()
print("Status:", res.status)
print("Location:", res.getheader('Location'))
print("Body:", res.read()[:500])

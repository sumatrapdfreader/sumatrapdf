import urllib.request
import time

url = f'https://lens.google.com/v3/upload?ep=cntpubb&hl=en&st={int(time.time()*1000)}&re=df&s=4'
boundary = '----WebKitFormBoundaryPager'
body = (f'--{boundary}\r\n'
        f'Content-Disposition: form-data; name="encoded_image"; filename="image.png"\r\n'
        f'Content-Type: image/png\r\n\r\n'
        f'TESTDATA\r\n'
        f'--{boundary}--\r\n').encode('utf-8')

headers = {
    'Content-Type': f'multipart/form-data; boundary={boundary}',
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/151.0.0.0 Safari/537.36',
    'Origin': 'https://lens.google.com',
    'Referer': 'https://lens.google.com/'
}

class NoRedirectHandler(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None

opener = urllib.request.build_opener(NoRedirectHandler)
urllib.request.install_opener(opener)

req = urllib.request.Request(url, data=body, headers=headers, method='POST')
try:
    with urllib.request.urlopen(req) as response:
        print('Status:', response.status)
        print('Location:', response.getheader('Location'))
        print('Body:', response.read().decode('utf-8')[:200])
except urllib.error.HTTPError as e:
    print('Error Status:', e.code)
    print('Location:', e.headers.get('Location'))
    print('Body:', e.read().decode('utf-8')[:200])

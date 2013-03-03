import os, sys, tempfile
from util import log

g_aws_access = None
g_aws_secret = None
g_bucket = None
g_conn = None

def import_boto():
  global S3Connection, bucket_lister
  try:
    from boto.s3.connection import S3Connection
    from boto.s3.bucketlistresultset import bucket_lister
  except:
    print("You need boto library (http://code.google.com/p/boto/)")
    print("svn checkout http://boto.googlecode.com/svn/trunk/ boto")
    print("cd boto; python setup.py install")
    raise

def set_secrets(access, secret):
  global g_aws_access, g_aws_secret
  g_aws_access = access
  g_aws_secret = secret

def set_bucket(bucket):
  global g_bucket
  g_bucket = bucket

def get_conn():
  global g_conn
  if g_conn is None:
    import_boto()
    g_conn = S3Connection(g_aws_access, g_aws_secret, True)
  return g_conn

def get_bucket():
  return get_conn().get_bucket(g_bucket)

def ul_cb(sofar, total):
  log("So far: %d, total: %d" % (sofar , total))

def upload_file_public(local_path, remote_path, silent=False):
  size = os.path.getsize(local_path)
  log("s3 upload %d bytes of '%s' as '%s'" % (size, local_path, remote_path))
  k = get_bucket().new_key(remote_path)
  if silent:
    k.set_contents_from_filename(local_path)
  else:
    k.set_contents_from_filename(local_path, cb=ul_cb)
  k.make_public()

def upload_data_public(data, remote_path):
  log("s3 upload %d bytes of data as '%s'" % (len(data), remote_path))
  k = get_bucket().new_key(remote_path)
  k.set_contents_from_string(data)
  k.make_public()

def upload_data_public_with_content_type(data, remote_path, silent=False):
  # writing to a file to force boto to set Content-Type based on file extension.
  # TODO: there must be a simpler way
  tmp_name = os.path.basename(remote_path)
  tmp_path = os.path.join(tempfile.gettempdir(), tmp_name)
  open(tmp_path, "w").write(data.encode("utf-8"))
  upload_file_public(tmp_path, remote_path, silent)
  os.remove(tmp_path)

def download_to_file(remote_path, local_path):
  log("s3 download '%s' as '%s'" % (remote_path, local_path))
  k = get_bucket().new_key(remote_path)
  k.get_contents_to_filename(local_path)

def list(s3dir):
  import_boto()
  b = get_bucket()
  return bucket_lister(b, s3dir)

def delete(remote_path):
  log("s3 delete '%s'" % remote_path)
  get_bucket().new_key(remote_path).delete()

def exists(remote_path):
  return get_bucket().get_key(remote_path)

def verify_doesnt_exist(remote_path):
  if not exists(remote_path):
    return
  log("'%s' already exists in s3" % remote_path)
  sys.exit(1)

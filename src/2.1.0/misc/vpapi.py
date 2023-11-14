#!/usr/bin/env python3
# Developed in 2023 by Xi Jinpwned Software.
# This code is dedicated to the public domain.

import hashlib, random, requests, shutil, time, zipfile

def new_session():
  session = requests.Session()
  session.headers.update({'User-Agent': 'juce'})
  return session

class TTSAuthAPI():
  DREAMTONICS_TTS_AUTH = "https://tts-auth.dreamtonics.com"

  def __init__(self, device_id=None, session=None,
                     api_url=DREAMTONICS_TTS_AUTH):
    if device_id is None:
      device_id = f"{random.randint(1000, 10000000)}.{random.randint(100000, 100000000)}"
    self.device_id = device_id
    if not session:
      session = requests.Session()
      session.headers.update({'User-Agent': 'juce'})
    self.session = session
    self.api_url = api_url

  def find_product(self, code, no_throw=False):
    resp = self.session.get(f'{self.api_url}/api/find_product', params={'code': code})
    resp.raise_for_status()
    if no_throw and resp.text == "status-220":
      return None
    return resp.json()

  def activate(self, code, product):
    resp = self.session.get(f'{self.api_url}/activate.php',
                            params={'operation': 'activation', 'code': code, 'device': self.device_id, 'product': product})
    resp.raise_for_status()
    if 'status' in resp.text:
      raise Exception("TTSAuth: " + resp.text)
    return resp.text

  def deactivate(self, key):
    resp = self.session.get(f'{self.api_url}/activate.php',
                            params={'operation': 'deactivation', 'key': key})
    if resp.text != 'status-100':
      raise Exception("TTSAuth: " + resp.text)
    return resp.text

  def request_obj(self, type, key, id, platform='windows'):
    resp = self.session.get(f'{self.api_url}/api/request_obj',
                            params={'type': type, 'key': key, 'id': id, 'platform': platform})
    resp.raise_for_status()
    return resp.json()

  def fetch(self, code):
    product = {}

    product['info'] = self.find_product(code)
    pdcv1_id = product['info']['product_hash']

    key = self.activate(code, pdcv1_id)
    pdcv1 = self.request_obj('pdcv1', key, pdcv1_id, 'windows')
    product['pdcv1'] = pdcv1
    product['ndcv1'] = []

    for narrator in pdcv1['narrator_list']:
      ndcv1_id = narrator['ndc_name']
      try:
        ndcv1 = self.request_obj('ndcv1', key, ndcv1_id, 'windows')
        product['ndcv1'].append(ndcv1)
      except exc as Exception:
        self.deactivate(key)
        raise exc
    self.deactivate(key)
    return product

def get_stamp(product):
  parts = [product['info']['product_hash']]
  for ndcv1 in product['ndcv1']:
    for instance in ndcv1['instance_list']:
      for component in instance['component_list']:
        parts.append(component['comp_id'])
  if 'code' in product:
    parts.append(product['code'])
  parts = ':'.join(parts)
  ver = f'v{product["pdcv1"]["narrator_list"][0]["latest_version"]}'
  return ver + "-" + hashlib.sha256(bytes(parts, 'utf-8')).hexdigest()[:10]

def make_package(product, version=None, filename=None, session=None):
  hash = product['info']['product_hash']

  if not filename:
    filename = f"{hash}.vppk"
  if not session:
    session = new_session()

  file_set = set()

  with zipfile.ZipFile(filename, 'w', zipfile.ZIP_DEFLATED) as pkg:
    info = product['info']
    pdcv1 = product['pdcv1']
    with pkg.open(f'{hash}.meta.json', 'w') as meta_fp:
      meta_fp.write(json.dumps({
        'info': info,
        'pdcv1': pdcv1,
        'ndcv1': product['ndcv1'],
        '_time': time.time(),
      }).encode('utf-8'))

    with pkg.open(f'package-config', 'w') as ppkg_fp:
      ppkg_fp.write(json.dumps({
        'vendorName': pdcv1['vendor_name'],
        'productName': pdcv1['prod_name']
      }).encode('utf-8'))

    with pkg.open(f'{hash}.ppkg', 'w') as ppkg_fp:
      ppkg_fp.write(json.dumps(pdcv1).encode('utf-8'))

    for ndcv1 in product['ndcv1']:
      instance_list = ndcv1['instance_list']
      instance_list.sort(reverse=True, key=lambda x: x['version'])
      if version:
        instance_list = list(filter(lambda x: x['version'] == version, instance_list))
        if len(instance_list) == 0: raise ValueError("Invalid version")
      instance = instance_list[0]

      files = instance['component_list']
      entrance = files[0] # TODO: better way?

      with pkg.open(f'{entrance["comp_id"]}.idc', 'w') as entr_fp:
        entr_fp.write(json.dumps({
          'narrator_id': ndcv1['ndc_name'],
          'entrance_component': entrance['comp_id'],
        }).encode('utf-8'))

      for entry in files:
        if entry["comp_id"] in file_set:
          continue
        print(f'Fetch {entry["comp_url"]}...')
        resp = session.get(entry['comp_url'])
        resp.raise_for_status()
        resp.raw.decode_content = True
        with pkg.open(f'{entry["comp_id"]}.sylapack', 'w') as fp:
          for chunk in resp.iter_content(1048576):
            fp.write(chunk)
          file_set.add(entry["comp_id"])

      pkg.comment = bytes(f"vendor: {pdcv1['vendor_name']}, product: {pdcv1['prod_name']}, stamp: {get_stamp(product)}", 'utf-8')

  return filename

if __name__ == '__main__':
  import json, os, sys
  v = TTSAuthAPI()
  if len(sys.argv) > 1 and sys.argv[1] == 'fetch':
    print(json.dumps(v.fetch(sys.argv[2].replace('-', ''))))
  elif len(sys.argv) > 1 and sys.argv[1] == 'package':
    print(make_package(json.load(open(sys.argv[2])), filename=(sys.argv[3] if len(sys.argv) > 3 else None)))
  elif len(sys.argv) > 1 and sys.argv[1] == 'catalog':
    code = sys.argv[2].replace('-', '')
    catalog_root = sys.argv[3] if len(sys.argv) > 3 else os.path.join('.', 'catalog')
    product = v.fetch(code)
    product['code'] = code
    stamp = get_stamp(product)
    root = os.path.join(catalog_root, f'{product["info"]["product_hash"][:8]}', f'{stamp}')
    os.makedirs(root, exist_ok=True)
    with open(os.path.join(root, 'fetch.json'), 'w') as fp: json.dump(product, fp, ensure_ascii=False)
    vppk = os.path.join(root, f'{stamp}-{product["pdcv1"]["prod_name"].replace(" ", "_")}.vppk')
    if not os.path.exists(vppk):
      print(make_package(product, filename=vppk))
  else:
    print('Commands: fetch, package, catalog')


import json
from itertools import chain

camera_file   = "data/camera_data.json"
output_folder = 'data/ori/'
orilist_loc   = 'data/'

# Reading the json as a dict
with open(camera_file) as json_data:
  data = json.load(json_data)

intrinsics = data['intrinsics'][0]['value']['ptr_wrapper']['data']
fl = intrinsics['focal_length']
width = intrinsics['width']
height = intrinsics['height']
pp = intrinsics['principal_point']

ox, oy = pp[0], pp[1]
fsx = width
fsy = height
rc = [width, height]
s = 0

extrinsics = data['extrinsics']   
orilist = open(orilist_loc+"orilist.txt", "w")

for entry in extrinsics:
  i =  entry['key']
  c =  entry['value']['center']
  r =  entry['value']['rotation']
  r =  list(chain.from_iterable(r))
  k = [fsx,   s, ox, 
         0, fsy, oy, 
         0,   0,  1]
  outfile = output_folder+"{0:03}.or".format(i)
  l1 = "$id {0:03}".format(i)
  l2 = "\n$rc{0:19}{1:19}".format(rc[0],rc[1])
  l3 = "\n$R"
  for ir in r:
    l3 = l3 + " {:18.8f}".format(ir)
  l4 = "\n$K"
  for ik in k:
    l4 = l4 + " {:18.8f}".format(ik)
  l5 = "\n$C"
  for ic in c:
    l5 = l5 + " {:18.10f}".format(ic)
  f = open(outfile, "w")
  for wline in [l1,l2,l3,l4,l5]:
    f.write(wline)
  f.close()
  orilist.write(outfile+"\n")
orilist.close()

##  Example: firstori.or
# $id none
# $rc               -1               -1
# $R       0.00000000       0.00000000       0.00000000       0.00000000       0.00000000       0.00000000       0.00000000       0.00000000       0.00000000
# $K       0.00000000       0.00000000       0.00000000       0.00000000       0.00000000       0.00000000       0.00000000       0.00000000       0.00000000
# $C         1.000000         2.000000         3.000000